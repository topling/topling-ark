
#include <terark/preproc.hpp>

typedef MyType<2, LargeSigma> TERARK_PP_CAT3(MyType, _2B_, LargeSigma);
typedef MyType<3, LargeSigma> TERARK_PP_CAT3(MyType, _3B_, LargeSigma);
typedef MyType<4, LargeSigma> TERARK_PP_CAT3(MyType, _4B_, LargeSigma);
typedef MyType<5, LargeSigma> TERARK_PP_CAT3(MyType, _5B_, LargeSigma);
typedef MyType<6, LargeSigma> TERARK_PP_CAT3(MyType, _6B_, LargeSigma);

typedef MyType<4, 16> BOOST_PP_CAT(MyType, _4B_16); // Sigma==16
typedef MyType<4, 32> BOOST_PP_CAT(MyType, _4B_32); // Sigma==32
typedef MyType<5, 32> BOOST_PP_CAT(MyType, _5B_32);

#undef MyType
#undef LargeSigma

