// terark_ptr.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <terark/refcount.hpp>
#include <boost/foreach.hpp>


using namespace std;
using namespace boost;

using terark::terark_ptr;

template<class T>
struct type2type
{
	typedef T type;
};

class IterBaseI_0
{
public:
	virtual bool hasNext() = 0;
	virtual void next() = 0;
	virtual void throw_x() = 0;
	virtual ~IterBaseI_0() {}
};
template<class Iter>
class IterImpl_0 : public IterBaseI_0
{
	Iter first, last;
public:
	IterImpl_0(Iter first, Iter last) : first(first), last(last) {}
	virtual bool hasNext() { return (first != last); }
	virtual void next() { ++first; }
	virtual void throw_x() { throw *first; }
};

template<class Iter>
IterBaseI_0* make_iter_xx(Iter first, Iter last)
{
	return new IterImpl_0<Iter>(first, last);
}

#define MFOREACH_0(var, c) \
	for (std::auto_ptr<IterBaseI_0> iter(make_iter_xx(c.begin(), c.end())); iter->hasNext(); iter->next()) \
	try { iter->throw_x(); } \
	catch (var)


#define MFOREACH_1(var, c) \


#define MFOREACH MFOREACH_0
class Base
{
public:
	virtual ~Base() {}
	int a;
};

class Derived : public Base
{
public:
	int b;

	~Derived()
	{
		int c = b;
	}
};

class NonDerived
{
public:
	int b;
};

void foo_bind();

int main(int argc, char* argv[])
{
	terark_ptr<int> a(new int(2));
	terark_ptr<NonDerived> nd(new NonDerived);
	terark_ptr<Derived> d0(new Derived);
	terark_ptr<Base> b0(new Base);
	terark_ptr<Base> b1(d0);
//	terark_ptr<Base> b2(nd); // should error, can not convert NonDerived* to Base*

	b0->a = d0->b;

	{
		terark_ptr<Base> b2((Base*)malloc(sizeof(Base)), &::free);
	}

	int aa = 0;
	float bb = reinterpret_cast<float&>(aa);
	void* vv = reinterpret_cast<void*&>(a);
	reinterpret_cast<terark_ptr<int>&>(vv).add_ref();

	string s1 = "abc";
	string s2 =
	boost::bind(static_cast<string& (string::*)(size_t, const char*)>(&string::insert),
 	boost::bind(static_cast<string& (string::*)(const char*)>(&string::append), _1, ">"), 0u, "<")
	(s1);

	foo_bind();

	return 0;
}

void foo_bind()
{
	std::vector<string> names;

	names.push_back("aaaa");
	names.push_back("bbbb");
	names.push_back("cccc");

	MFOREACH(string& x, names)
	{
	//	x = "<" + x + ">";
		cout << x << endl;
	}
	MFOREACH(string& x, names)
	{
		cout << x << endl;
	}
	int aaaa = 0;

// 	for_each(names.begin(), names.end(),
// 		bind(static_cast<string& (string::*)(size_t, const char*)>(&string::insert),
// 		bind(static_cast<string& (string::*)(const char*)>(&string::append), _1, ">"), 0u, "<")
// 		);
//	function<void(string&)> f1 = _1 = var(string("<")) + _1 + string(">");
//	for_each(names.begin(), names.end(), _1 = var(string("<")) + _1 + string(">"));
//	(std::cout << _1 << " " << _3 << " " << _2 << "!\n")("Hello","friend","my");

//	_1 = var(string("<")) + _1 + string(">");


}

