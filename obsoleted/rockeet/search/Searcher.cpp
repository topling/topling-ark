#include <vector>
// #include <boost/intrusive_ptr.hpp>
//
// #include <terark/io/MemStream.hpp>
// #include <terark/util/refcount.hpp>
// #include <terark/util/DataBuffer.hpp>
//
// #include <rockeet/doc/Schema.hpp>
#include "Searcher.hpp"
#include <terark/set_op.hpp>

namespace terark { namespace rockeet {

	QueryResultPtr OrExpression::eval()
	{
		if (subs.size() == 0)
		{
			throw std::logic_error("OrExpression::eval(), empty");
		}
		else if (subs.size() == 1)
		{
			return subs[0]->eval();
		}
		else if (subs.size() == 2)
		{
			QueryResultPtr x = subs[0]->eval();
			QueryResultPtr y = subs[1]->eval();
			TempQueryResult* result = new TempQueryResult();

			x->initCursor();
			y->initCursor();
			while (!x->empty() && !y->empty())
			{
				if (x->docID < y->docID)
				{
					result->put(x->docID);
					x->pop_front();
				}
				else if (y->docID < x->docID)
				{
					result->put(y->docID);
					y->pop_front();
				}
				else
				{
					result->put(x->docID);
					x->pop_front();
					y->pop_front();
				}
			}
			while (!x->empty())
			{
				result->put(x->docID);
				x->pop_front();
			}
			while (!y->empty())
			{
				result->put(y->docID);
				y->pop_front();
			}
			return result;
		}
		else
		{
// 			typedef std::vector<TT_PAIR(std::vector<uint64_t>::iterator) > iter_vec_t;
// 			iter_vec_t iv;
// 			for (ptrdiff_t i = 0; i != vv.size(); ++i)
// 			{
// 			//	if (!vv[i].empty())
// 			//		iv.push_back()
// 				iv.push_back(make_pair(vv[i].begin(), vv[i].end()));
// 			}
// 			typedef multi_way::HeapMultiWay<iter_vec_t::iterator> mw_t;
// 			mw_t mw(iv.begin(), iv.end());
// 			if (strcmp(op, "or") == 0)
// 				mw.union_set(back_inserter(result));
// 			else if (strcmp(op, "and") == 0)
// 				mw.intersection(back_inserter(result));
// 			else
// 				throw runtime_error("not supported op");
			abort();
			throw std::runtime_error(BOOST_CURRENT_FUNCTION);
		}
	}

//////////////////////////////////////////////////////////////////////////

	QueryResultPtr AndExpression::eval()
	{
		if (subs.size() == 0)
		{
			throw std::logic_error("AndExpression::eval(), empty");
		}
		else if (subs.size() == 1)
		{
			return subs[0]->eval();
		}
		else if (subs.size() == 2)
		{
			QueryResultPtr x = subs[0]->eval();
			QueryResultPtr y = subs[1]->eval();
			TempQueryResult* result = new TempQueryResult();

			x->initCursor();
			y->initCursor();
			while (!x->empty() && !y->empty())
			{
				if (x->docID < y->docID)
				{
					x->pop_front();
				}
				else if (y->docID < x->docID)
				{
					y->pop_front();
				}
				else
				{
					result->put(x->docID);
					x->pop_front();
					y->pop_front();
				}
			}
			return result;
		}
		else
		{
// 			typedef std::vector<TT_PAIR(std::vector<uint64_t>::iterator) > iter_vec_t;
// 			iter_vec_t iv;
// 			for (ptrdiff_t i = 0; i != vv.size(); ++i)
// 			{
// 			//	if (!vv[i].empty())
// 			//		iv.push_back();
// 				iv.push_back(make_pair(vv[i].begin(), vv[i].end()));
// 			}
// 			typedef multi_way::HeapMultiWay<iter_vec_t::iterator> mw_t;
// 			mw_t mw(iv.begin(), iv.end());
// 			if (strcmp(op, "or") == 0)
// 				mw.union_set(back_inserter(result));
// 			else if (strcmp(op, "and") == 0)
// 				mw.intersection(back_inserter(result));
// 			else
// 				throw runtime_error("not supported op");
			abort();
			throw std::runtime_error(BOOST_CURRENT_FUNCTION);
		}
	}

	QueryResultPtr SubtractExpression::eval()
	{
		QueryResultPtr x = left->eval();
		QueryResultPtr y = right->eval();
		TempQueryResult* result = new TempQueryResult();

		x->initCursor();
		y->initCursor();
		while (!x->empty() && !y->empty())
		{
			if (x->docID < y->docID)
			{
				result->put(x->docID);
				x->pop_front();
			}
			else if (y->docID < x->docID)
			{
				y->pop_front();
			}
			else
			{
				x->pop_front();
				y->pop_front();
			}
		}
		while (!x->empty())
		{
			result->put(x->docID);
			x->pop_front();
		}
		return result;
	}

	QueryResultPtr XorExpression::eval()
	{
		QueryResultPtr x = left->eval();
		QueryResultPtr y = right->eval();
		TempQueryResult* result = new TempQueryResult();

		x->initCursor();
		y->initCursor();
		while (!x->empty() && !y->empty())
		{
			if (x->docID < y->docID)
			{
				result->put(x->docID);
				x->pop_front();
			}
			else if (y->docID < x->docID)
			{
				result->put(y->docID);
				y->pop_front();
			}
			else
			{
				x->pop_front();
				y->pop_front();
			}
		}
		while (!x->empty())
		{
			result->put(x->docID);
			x->pop_front();
		}
		while (!y->empty())
		{
			result->put(y->docID);
			y->pop_front();
		}
		return result;
	}


} } // namespace terark::rockeet
