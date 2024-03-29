#include "as/Parse.h"

#include <mn/IO.h>
#include <mn/Defer.h>

namespace as
{
	struct Parser
	{
		Src* src;
		mn::Buf<Tkn> tkns;
		size_t ix;
	};

	inline static Parser
	parser_new(Src* src)
	{
		Parser self{};
		self.src = src;
		self.tkns = mn::buf_clone(src->tkns);
		self.ix = 0;
		return self;
	}

	inline static void
	parser_free(Parser& self)
	{
		mn::buf_free(self.tkns);
	}

	inline static void
	destruct(Parser& self)
	{
		parser_free(self);
	}

	inline static Tkn
	parser_look(Parser* self, size_t k)
	{
		if(self->ix + k >= self->tkns.count)
			return Tkn{};
		return self->tkns[self->ix + k];
	}

	inline static Tkn
	parser_look(Parser *self)
	{
		return parser_look(self, 0);
	}

	inline static Tkn
	parser_look_kind(Parser* self, Tkn::KIND k)
	{
		Tkn t = parser_look(self);
		if(t.kind == k)
			return t;
		return Tkn{};
	}

	inline static Tkn
	parser_eat(Parser* self)
	{
		if(self->ix >= self->tkns.count)
			return Tkn{};
		return self->tkns[self->ix++];
	}

	inline static Tkn
	parser_eat_kind(Parser* self, Tkn::KIND kind)
	{
		Tkn t = parser_look(self);
		if(t.kind == kind)
			return parser_eat(self);
		return Tkn{};
	}

	inline static Tkn
	parser_eat_must(Parser* self, Tkn::KIND kind)
	{
		if(self->ix >= self->tkns.count)
		{
			src_err(
				self->src,
				mn::strf("expected '{}' but found EOF", Tkn::NAMES[kind])
			);
			return Tkn{};
		}

		Tkn tkn = parser_eat(self);
		if(tkn.kind == kind)
			return tkn;

		src_err(
			self->src,
			tkn,
			mn::strf("expected '{}' but found '{}'", Tkn::NAMES[kind], tkn.str)
		);
		return Tkn{};
	}

	inline static bool
	is_reg(const Tkn& tkn)
	{
		return (tkn.kind == Tkn::KIND_KEYWORD_R0 ||
				tkn.kind == Tkn::KIND_KEYWORD_R1 ||
				tkn.kind == Tkn::KIND_KEYWORD_R2 ||
				tkn.kind == Tkn::KIND_KEYWORD_R3 ||
				tkn.kind == Tkn::KIND_KEYWORD_R4 ||
				tkn.kind == Tkn::KIND_KEYWORD_R5 ||
				tkn.kind == Tkn::KIND_KEYWORD_R6 ||
				tkn.kind == Tkn::KIND_KEYWORD_R7 ||
				tkn.kind == Tkn::KIND_KEYWORD_IP);
	}

	inline static Tkn
	parser_reg(Parser* self)
	{
		auto op = parser_look(self);
		if (is_reg(op))
		{
			return parser_eat(self);
		}
		
		src_err(self->src, op, mn::strf("expected a register but found '{}'", op.str));
		return Tkn{};
	}

	inline static Tkn
	parser_const(Parser* self)
	{
		auto op = parser_look(self);
		if (op.kind == Tkn::KIND_INTEGER ||
			op.kind == Tkn::KIND_FLOAT)
		{
			return parser_eat(self);
		}
		
		src_err(self->src, op, mn::strf("expected a constant but found '{}'", op.str));
		return Tkn{};
	}

	inline static bool
	is_load(const Tkn& tkn)
	{
		return (tkn.kind == Tkn::KIND_KEYWORD_I8_LOAD ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_LOAD ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_LOAD ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_LOAD ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_LOAD ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_LOAD ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_LOAD ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_LOAD);
	}

	inline static bool
	is_arithmetic(const Tkn& tkn)
	{
		return (tkn.kind == Tkn::KIND_KEYWORD_I8_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_ADD ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_SUB ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_MUL ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_DIV ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_DIV ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_DIV ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_DIV ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_DIV ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_DIV ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_DIV ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_DIV);
	}

	inline static bool
	is_cond_jump(const Tkn& tkn)
	{
		return (tkn.kind == Tkn::KIND_KEYWORD_I8_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_JE ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_JNE ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_JL ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_JLE ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_JG ||
				tkn.kind == Tkn::KIND_KEYWORD_I8_JGE ||
				tkn.kind == Tkn::KIND_KEYWORD_I16_JGE ||
				tkn.kind == Tkn::KIND_KEYWORD_I32_JGE ||
				tkn.kind == Tkn::KIND_KEYWORD_I64_JGE ||
				tkn.kind == Tkn::KIND_KEYWORD_U8_JGE ||
				tkn.kind == Tkn::KIND_KEYWORD_U16_JGE ||
				tkn.kind == Tkn::KIND_KEYWORD_U32_JGE ||
				tkn.kind == Tkn::KIND_KEYWORD_U64_JGE);
	}

	inline static Ins
	parser_ins(Parser* self)
	{
		Ins ins{};

		Tkn op = parser_look(self);
		if (is_load(op))
		{
			ins.op = parser_eat(self);
			ins.dst = parser_reg(self);
			ins.src = parser_const(self);
		}
		else if (is_arithmetic(op))
		{
			ins.op = parser_eat(self);
			ins.dst = parser_reg(self);
			ins.src = parser_reg(self);
		}
		else if (is_cond_jump(op))
		{
			ins.op = parser_eat(self);
			ins.dst = parser_reg(self);
			ins.src = parser_reg(self);
			ins.lbl = parser_eat_must(self, Tkn::KIND_ID);
		}
		else if (op.kind == Tkn::KIND_KEYWORD_JMP)
		{
			ins.op = parser_eat(self);
			ins.lbl = parser_eat_must(self, Tkn::KIND_ID);
		}
		// label
		else if (op.kind == Tkn::KIND_ID)
		{
			ins.op = parser_eat(self);
			parser_eat_must(self, Tkn::KIND_COLON);
		}
		else if(op.kind == Tkn::KIND_KEYWORD_HALT)
		{
			ins.op = parser_eat(self);
		}

		return ins;
	}

	inline static Proc
	parser_proc(Parser* self)
	{
		parser_eat_must(self, Tkn::KIND_KEYWORD_PROC);
		auto proc = proc_new();
		proc.name = parser_eat_must(self, Tkn::KIND_ID);

		while (parser_look_kind(self, Tkn::KIND_KEYWORD_END) == false)
		{
			auto ins = parser_ins(self);
			if (ins.op)
				mn::buf_push(proc.ins, ins);
			else
				break;
		}

		parser_eat_kind(self, Tkn::KIND_KEYWORD_END);

		return proc;
	}

	// API
	bool
	parse(Src* src)
	{
		auto parser = parser_new(src);
		mn_defer(parser_free(parser));

		while(parser.ix < parser.tkns.count)
		{
			auto proc = parser_proc(&parser);
			if (src_has_err(src)) break;
			mn::buf_push(src->procs, proc);
		}

		return src_has_err(src) == false;
	}

	mn::Str
	proc_dump(Src* self, mn::Allocator allocator)
	{
		auto out = mn::memory_stream_new(allocator);
		mn_defer(mn::memory_stream_free(out));

		for(const auto& proc: self->procs)
		{
			mn::print_to(out, "PROC {}\n", proc.name.str);
			for(const auto& ins: proc.ins)
			{
				if (is_load(ins.op) ||
					is_arithmetic(ins.op))
				{
					mn::print_to(out, "  {} {} {}\n", ins.op.str, ins.dst.str, ins.src.str);
				}
				else if(is_cond_jump(ins.op))
				{
					mn::print_to(out, "  {} {} {} {}\n", ins.op.str, ins.dst.str, ins.src.str, ins.lbl.str);
				}
				else if(ins.op.kind == Tkn::KIND_KEYWORD_JMP)
				{
					mn::print_to(out, "  {} {}\n", ins.op.str, ins.lbl.str);
				}
				else if(ins.op.kind == Tkn::KIND_ID)
				{
					mn::print_to(out, "{}:\n", ins.op.str);
				}
				else if(ins.op.kind == Tkn::KIND_KEYWORD_HALT)
				{
					mn::print_to(out, "  {}\n", ins.op.str);
				}
				else
				{
					mn::print_to(out, "  INVALID OP\n");
				}
			}
			mn::print_to(out, "END\n");
		}

		return memory_stream_str(out);
	}
}