#include "as/Scan.h"

#include <mn/IO.h>
#include <mn/Rune.h>

namespace as
{
	struct Scanner
	{
		Src* src;
		const char* it;
		mn::Rune c;
		Pos pos;
	};

	inline static Scanner
	scanner_new(Src* src)
	{
		Scanner self{};
		self.src = src;
		self.it = begin(src->content);
		self.c = mn::rune_read(self.it);
		self.pos = Pos{1, 0};

		src_line_begin(self.src, self.it);
		return self;
	}

	inline static bool
	scanner_eof(const Scanner* self)
	{
		return self->it >= end(self->src->content);
	}

	inline static bool
	is_whitespace(mn::Rune c)
	{
		return (
			c == ' ' ||
			c == '\n' ||
			c == '\r' ||
			c == '\t' ||
			c == '\v'
		);
	}

	inline static bool
	is_letter(mn::Rune c)
	{
		return (mn::rune_is_letter(c) || c == '_');
	}

	inline static bool
	is_digit(mn::Rune c)
	{
		return (c >= '0' && c <= '9');
	}

	inline static bool
	scanner_eat(Scanner* self)
	{
		if(scanner_eof(self))
			return false;

		auto prev_it = self->it;
		auto prev_c = mn::rune_read(self->it);

		self->it = mn::rune_next(self->it);
		self->c = mn::rune_read(self->it);

		self->pos.col++;

		if(prev_c == '\n')
		{
			self->pos.col = 1;
			self->pos.line++;
			src_line_end(self->src, prev_it);
			src_line_begin(self->src, self->it);
		}
		return true;
	}

	inline static void
	scanner_skip_whitespaces(Scanner* self)
	{
		while(is_whitespace(self->c))
			if(scanner_eat(self) == false)
				break;
	}

	inline static const char*
	scanner_id(Scanner* self)
	{
		auto begin_it = self->it;
		while(is_letter(self->c) || is_digit(self->c) || self->c == '.')
			if(scanner_eat(self) == false)
				break;
		return mn::str_intern(self->src->str_table, begin_it, self->it);
	}

	inline static int
	digit_value(mn::Rune c)
	{
		if (is_digit(c))
			return c - '0';
		else if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return 16;
	}

	inline static bool
	scanner_digits(Scanner *self, int base)
	{
		bool found = false;
		while(digit_value(self->c) < base)
		{
			found = true;
			if(scanner_eat(self) == false)
				break;
		}
		return found;
	}

	inline static void
	scanner_num(Scanner* self, Tkn& tkn)
	{
		const char* begin_it = self->it;
		Pos begin_pos = self->pos;
		tkn.kind = Tkn::KIND_INTEGER;

		if (self->c == '-' || self->c == '+')
			scanner_eat(self);

		if(self->c == '0')
		{
			const char* backup_it = self->it;
			Pos backup_pos = self->pos;
			scanner_eat(self); //for the 0

			int base = 0;
			switch(self->c)
			{
			case 'b': case 'B': base = 2; break;
			case 'o': case 'O': base = 8; break;
			case 'd': case 'D': base = 10; break;
			case 'x': case 'X': base = 16; break;
			default: break;
			}

			if(base != 0)
			{
				if(scanner_digits(self, base) == false)
				{
					src_err(self->src, Err{
						begin_pos,
						Rng{begin_it, self->it},
						mn::strf("illegal int literal {}", self->c)
					});
				}
				tkn.str = str_intern(self->src->str_table, begin_it, self->it);
				return;
			}

			//this is not a 0x number
			self->it = backup_it;
			self->c = mn::rune_read(self->it);
			self->pos = backup_pos;
		}

		//since this is not a 0x number
		//it might be an integer or float so parse a decimal number anyway
		if(scanner_digits(self, 10) == false)
		{
			src_err(self->src, Err{
				begin_pos,
				Rng{begin_it, self->it},
				mn::strf("illegal int literal {}", self->c)
			});
		}

		//float part
		if(self->c == '.')
		{
			tkn.kind = Tkn::KIND_FLOAT;
			scanner_eat(self); //for the .
			//parse the after . part
			if(scanner_digits(self, 10) == false)
			{
				src_err(self->src, Err{
					begin_pos,
					Rng{begin_it, self->it},
					mn::strf("illegal float literal {}", self->c)
				});
			}
		}

		//scientific notation part
		if(self->c == 'e' || self->c == 'E')
		{
			tkn.kind = Tkn::KIND_FLOAT;
			scanner_eat(self); //for the e
			if(self->c == '-' || self->c == '+')
				scanner_eat(self);
			if(scanner_digits(self, 10) == false)
			{
				src_err(self->src, Err{
					begin_pos,
					Rng{begin_it, self->it},
					mn::strf("illegal float literal {}", self->c)
				});
			}
		}

		//finished the parsing of the number whether it's a float or int
		tkn.str = str_intern(self->src->str_table, begin_it, self->it);
	}

	inline static bool
	case_insensitive_cmp(const char* a, const char* b)
	{
		auto a_count = mn::rune_count(a);
		auto b_count = mn::rune_count(b);
		if(a_count != b_count)
			return false;
		for(size_t i = 0; i < a_count; ++i)
		{
			if(mn::rune_lower(mn::rune_read(a)) != mn::rune_lower(mn::rune_read(b)))
			{
				return false;
			}
			a = mn::rune_next(a);
			b = mn::rune_next(b);
		}
		return true;
	}

	inline static Tkn
	scanner_tkn(Scanner* self)
	{
		scanner_skip_whitespaces(self);

		if(scanner_eof(self))
			return Tkn{};

		Tkn tkn{};
		tkn.pos = self->pos;
		tkn.rng.begin = self->it;
		if(is_letter(self->c))
		{
			tkn.kind = Tkn::KIND_ID;
			tkn.str = scanner_id(self);

			//let's loop over all the keywords and check them
			for(size_t i = size_t(Tkn::KIND_KEYWORDS__BEGIN + 1);
				i < size_t(Tkn::KIND_KEYWORDS__END);
				++i)
			{
				if(case_insensitive_cmp(tkn.str, Tkn::NAMES[i]))
				{
					tkn.kind = Tkn::KIND(i);
					break;
				}
			}
		}
		else if(is_digit(self->c))
		{
			scanner_num(self, tkn);
		}
		else if(self->c == '-' || self->c == '+')
		{
			auto next = mn::rune_read(mn::rune_next(self->it));
			if(is_digit(next))
				scanner_num(self, tkn);
		}
		else
		{
			auto c = self->c;
			Pos begin_pos = self->pos;
			scanner_eat(self);
			bool no_intern = false;
			
			switch(c)
			{
			case ':':
				tkn.kind = Tkn::KIND_COLON;
				tkn.str = ":";
				break;
			default:
				src_err(self->src, begin_pos, mn::strf("illegal character {}", self->c));
				break;
			}

			if (no_intern == false)
				tkn.str = mn::str_intern(self->src->str_table, tkn.rng.begin, self->it);
		}
		tkn.rng.end = self->it;
		return tkn;
	}

	// API
	bool
	scan(Src* src)
	{
		auto scanner = scanner_new(src);
		while(true)
		{
			if(auto tkn = scanner_tkn(&scanner))
				src_tkn(src, tkn);
			else
				break;
		}
		return src_has_err(src) == false;
	}
}