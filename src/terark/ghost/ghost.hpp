#pragma once

#include <vector>

namespace terark { namespace ghost {

	class rule;
	class rule_i;
	struct context;

	class lexer
	{
	public:
		int nexttoken(context* ctx) const; // return token type
	};

	class grammar
	{
		std::vector<rule_i*> allrule;
		int maxid;
		lexer* lex;
//		void syntax_error(const char* msg);
		void compile();
	};

	struct context
	{
		grammar* g;
		lexer* lex;
		const char* text;
		const char* pos;
		const char* tokenstr;
		int textlen;
		int tokenlen;
		int tokentype;

		void readtoken();
	};

	class action
	{
	public:
		virtual void exec(context& ctx) const = 0;

		template<class Functor>
		static action* create(const Functor& f);
	};

	template<class Functor>
	class func2action : public action {
		Functor f;
	public:
		func2action(const Functor& f) : f(f) {}
		void exec(context& ctx) const { f(ctx); }
	};
	template<class Functor>
	static action* action::create(const Functor& f)
	{
		return new func2action<Functor>(f);
	}


	class rule_i // rule interface
	{
	public:
		int id;
		int first;
		action* act;
	public:
		rule_i() : act() {}
		virtual ~rule_i();

		virtual void parse(context& ctx) = 0;
	};

	class rule
	{
		rule_i* r;
	public:
		void parse(const char* str, int len);

		rule operator&(rule y);
		rule operator|(rule y);
		rule operator*();

		template<class Func>
		rule operator[](const Func& fun) {
			r->act = new Func(fun);
		}
	};

//########################################################################
//########################################################################

	enum {
		tt_start = 257,
		tt_number,
		tt_real,
		tt_int,
		tt_string,
	};

// pre-caned token types
	class token_int : rule_i
	{
	public:
		void parse(context& ctx);
		struct assign_int : action {
			int* p;
			assign_int(int& x) : p(&x) {}
			void exec(context& ctx) const;
		};
		struct assign_uint : action {
			unsigned* p;
			assign_uint(unsigned& x) : p(&x) {}
			void exec(context& ctx) const;
		};
		struct assign_long : action {
			long* p;
			assign_long(long& x) : p(&x) {}
			void exec(context& ctx) const;
		};
		struct assign_ulong : action {
			unsigned long* p;
			assign_ulong(unsigned long& x) : p(&x) {}
			void exec(context& ctx) const;
		};
		struct assign_ll: action {
			long long* p;
			assign_ll(long long& x) : p(&x) {}
			void exec(context& ctx) const;
		};
		struct assign_ull: action {
			unsigned long long* p;
			assign_ull(unsigned long long& x) : p(&x) {}
			void exec(context& ctx) const;
		};
		void set_action(int& x) { this->act = new assign_int(x); }
		void set_action(unsigned& x) { this->act = new assign_uint(x); }
		void set_action(long& x) { this->act = new assign_long(x); }
		void set_action(unsigned long& x) { this->act = new assign_ulong(x); }
		void set_action(long long& x) { this->act = new assign_ll(x); }
		void set_action(unsigned long long& x) { this->act = new assign_ull(x); }
	};

	namespace {
		template<class Token>
		struct token_to_rule {
			template<class ActionFunc>
			rule operator[](const ActionFunc& af) const {
				Token* tp = new Token;
				tp->act = action::create(f);
				rule_i* r = tp;
				return rule(r);
			}
		};

	}

} } // terark::ghost

