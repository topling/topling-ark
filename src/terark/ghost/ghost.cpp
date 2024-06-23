#include "ghost.hpp"

namespace terark { namespace ghost {

	struct Elem
	{
		rule*   r;
		rule_i* i;
	};

	class concat : public rule_i
	{
	public:
		std::vector<Elem> seq;
		void parse(context& ctx) const
		{
			for (int i = 0, n = (int)seq.size(); i < n; ++i) {
				seq[i].i->parse(ctx);
			}
		}
	};

	class alternative : public rule_i
	{
	public:
		std::vector<Elem> alt;
		void parse(context& ctx) const
		{
			for (int i = 0, n = (int)alt.size(); i < n; ++i) {
				if (alt[i].i->first == ctx.tokentype) {
					alt[i].i->parse(ctx);
					break;
				}
			}
		}
	};

	class zero_or_many : public rule_i
	{
	public:
		Elem embed;
		void parse(context& ctx)
		{
			while (ctx.tokentype == this->first) {
				embed.i->act->exec(ctx);
				ctx.readtoken();
			}
		}
	};

	class one_or_many : public rule_i
	{
	public:
		Elem embed;
		void parse(context& ctx) const
		{
			if (ctx.tokentype == this->first) {
				do {
					embed.i->act->exec(ctx);
					ctx.readtoken();
				} while (ctx.tokentype == this->first);
			}
			else {
				fprintf(stderr, "syntax error in one_or_many\n");
			}
		}
	};

	rule_i::~rule_i() {

	}

	rule rule::operator*()
	{
		zero_or_many* s = new zero_or_many;
		return rule(s);
	}

	rule rule::operator&(rule& y)
	{
		if (alternative* alt = dynamic_cast<alternative*>(this->r)) {
			alt->alt.push_back(y);
		}
		else if (concat* cons = dynamic_cast<concat*>(this->r)) {
			Elem ri; ri.r = y;
			cons->seq.push_back(y);
		}
		else {
			concat* cons = new concat;
			cons->seq.push_back(y);
		}
		return *this;
	}
	rule rule::operator|(rule y)
	{
		alternative* alt = dynamic_cast<alternative*>(r);
		if (NULL == alt) {
			alt = new alternative;
			alt->back().push_back(rule(r));
			r = alt;
		}
		alt->alt.push_back(y);
		return *this;
	}
// 	template<class Func>
// 	rule& operator[](Func fun) { act = new Func(fun); }
// 	rule& operator*();


	void sample() {
		rule expr;
		rule term = int_p | '(' & expr & ')';
		rule fact = term & *('+' & term);
		expr = fact & *('*' & fact);
	}

} } // terark::ghost

#endif // __terark_ghost_ghost__
