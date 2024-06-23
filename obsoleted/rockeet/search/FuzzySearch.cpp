#include <vector>
// #include <boost/intrusive_ptr.hpp>
//
// #include <terark/io/MemStream.hpp>
// #include <terark/util/refcount.hpp>
// #include <terark/util/DataBuffer.hpp>
//
// #include <rockeet/doc/Schema.hpp>
#include "FuzzySearch.hpp"
#include <terark/set_op.hpp>

namespace terark { namespace rockeet {

	void HitExtractor::mixAnd(uint64_t docID, HitInfo** hits, int nHits, AutoGrownMemIO& output)
	{
		output.ensureWrite(&docID, 8);
	}

	void HitExtractor::mixOr(uint64_t docID, HitInfo** hits, int nHits, AutoGrownMemIO& output)
	{
		output.ensureWrite(&docID, 8);
	}

	void HitExtractor::mixMinMatch(uint64_t docID, HitInfo** hits, int nHits, AutoGrownMemIO& output)
	{
		output.ensureWrite(&docID, 8);
	}

	//////////////////////////////////////////////////////////////////////////
/*
	void FuzzyOrExpr::eval(QueryResult& result)
	{
		if (subs.size() == 1)
		{
			subs[0]->eval(result);
		}
		else if (subs.size() == 2)
		{
			QueryResult temp0, temp1;
			subs[0]->eval(temp0);
			subs[1]->eval(temp1);

			HitInfoReader& x = *subs[0]->hitInfoReader;
			HitInfoReader& y = *subs[1]->hitInfoReader;
			while (!x.empty() && !y.empty())
			{
				if (x.docID < y.docID)
				{
					result.ensureWrite(&x.docID, 8);
					result.ensureWrite(x.hi.pVal, x.hi.cVal);
					x.pop_front();
				}
				else if (y.docID < x.docID)
				{
					result.ensureWrite(&y.docID, 8);
					result.ensureWrite(y.hi.pVal, y.hi.cVal);
					y.pop_front();
				}
				else
				{
					result.ensureWrite(&x.docID, 8);
					HitInfo* hits[2] = {&x.hi, &y.hi};
					hitExtractor->mixOr(x.docID, hits, 2, result);
					x.pop_front();
					y.pop_front();
				}
			}
			while (!x.empty())
			{
				result.ensureWrite(&x.docID, 8);
				result.ensureWrite(x.hi.pVal, x.hi.cVal);
				x.pop_front();
			}
			while (!y.empty())
			{
				result.ensureWrite(&y.docID, 8);
				result.ensureWrite(y.hi.pVal, y.hi.cVal);
				y.pop_front();
			}
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
		}
	}
*/
	//////////////////////////////////////////////////////////////////////////

	std::vector<uint64_t> FuzzySearch::search(const std::string& strQuery)
	{
		std::vector<uint64_t> vec;
		for (const char* p = &strQuery[0]; *p; ++p)
		{

		}
		return vec;
	}


} } // namespace terark::rockeet
