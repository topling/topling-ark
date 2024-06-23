//############################################################################
	struct BlockChar {
		BlockID b;
		short c;
		BlockChar(BlockID b, short c) : b(b), c(c){}
	};
	template<class E_Gen> void refine0(E_Gen egen) {
		assert(P.size() >= 1);
		struct WaitingSet : valvec<BlockChar> {};
		WaitingSet   W;
		static_assert(sizeof(static_bitmap<256>) == 32, "static_bitmap Error");
		valvec<static_bitmap<256> > isInW(L.size(), false);
		auto putW = [&W,&isInW](BlockID b, byte_t ch) {
			W.push_back(BlockChar(b, ch));
			isInW[b].set1(ch);
		};
		auto takeW = [&W,&isInW]() -> BlockChar {
			BlockChar bc = W.back();
			W.pop_back();
			isInW[bc.b].set0(bc.c);
			return bc;
		};
		smallmap<Splitter> C_1(256); // set of p which move(p,ch) is in Block C
		valvec<BlockID> bls; // candidate splitable Blocks
		long iterations = 0;
		long num_split = 0;
		long non_split = 0;
//		for (int c = 0; c < 256; ++c) putW(P.size()==1 ? 0 : minBlock(0,1), c);
		for (int b = 0; b < (int)P.size(); ++b)
			for (int c = 0; c < 256; ++c)
				putW(b, c);

		while (!W.empty()) {
			const BlockChar bc = takeW();
			{
				C_1.resize0();
				StateID head = P[bc.b].head;
				StateID curr = head;
				do {
					egen(curr, C_1);
					curr = L[curr].next;
				} while (curr != head);
			}
if (!C_1.exists(bc.c))
	continue;
AU_TRACE("iteration=%03ld, size[P=%ld, W=%ld, C_1=%ld], bc[b=%ld, c=%c]\n"
        , iterations, (long)P.size(), (long)W.size(), (long)C_1.size()
        , (long)bc.b, bc.c);
//-split----------------------------------------------------------------------
	Splitter& E = C_1.bykey(bc.c);
//  E.sort();
    size_t Esize = E.size();
//----Generate candidate splitable set: bls---------------
{
	bls.reserve(Esize);
	bls.resize(0);
	size_t iE1 = 0, iE2 = 0;
	for (; iE1 < Esize; ++iE1) {
		StateID sE = E[iE1];
		BlockID blid = L[sE].blid;
		if (P[blid].blen != 1) { // remove State which blen==1 in E
			E[iE2++] = sE;
			bls.unchecked_push_back(blid);
		}
		assert(P[blid].blen > 0);
	}
	E.resize(Esize = iE2);
	bls.resize(std::unique(bls.begin(), bls.end()) - bls.begin());
	if (bls.size() == 1) {
		BlockID blid = bls[0];
//        printSplitter("  splitter", E);
		if (E.size() == P[blid].blen)
			continue; // not a splitter
		assert(E.size() < P[blid].blen);
	//	printf("single splitter\n");
	} else {
		std::sort(bls.begin(), bls.end());
		bls.resize(std::unique(bls.begin(), bls.end()) - bls.begin());
	}
//	if (bls.size() > 1) printf("bls.size=%ld\n", (long)bls.size());
}
//----End Generate candidate splitable set: bls---------------
    for (size_t i = 0; i < bls.size(); ++i) {
        assert(Esize > 0);
        const BlockID B_E = bls[i];   // (B -  E) take old BlockID of B
        const BlockID BxE = P.size(); // (B /\ E) take new BlockID
		const Block B = P[B_E]; // copy, not ref
        assert(B.blen > 1);
        printSplitter("  splitter", E);
//        printBlock(B_E);
        P.push_back();  // (B /\ E) will be in P.back() -- BxE
        size_t iE1 = 0, iE2 = 0;
        for (; iE1 < Esize; ++iE1) {
            StateID sE = E[iE1];
            if (L[sE].blid == B_E) {
                del(B_E, sE); // (B -  E) is in B_E
                add(BxE, sE); // (B /\ E) is in BxE
            } else
			   	E[iE2++] = sE; // E - B
        }
        E.resize(Esize = iE2); // let E = (E - B)
        assert(P[BxE].blen); // (B /\ E) is never empty
        assert(P[B_E].blen < B.blen); // assert P[B_E] not roll down
        if (P[B_E].blen == 0) { // (B - E) is empty
            P[B_E] = B; // restore the block
            fix_blid(B_E);
            P.pop_back(); // E is not a splitter for B
            non_split++;
        } else {
 //       printBlock(B_E);
 //       printBlock(BxE);
			num_split++;
			BlockID minB = minBlock(B_E, BxE);
/*			printf("B_E:%ld, n=%ld; BxE:%ld, n=%ld\n"
					, (long)B_E, (long)P[B_E].blen
					, (long)BxE, (long)P[BxE].blen
					);
*/			for (int ch = 0; ch < 256; ++ch)
				if (isInW[B_E].test(ch))
					putW(BxE, ch);
				else
					putW(minB, ch);
		}
    } // for
//-end split------------------------------------------------------------------
            ++iterations;
        }
        if (P[0].head != 0) { // initial state is not in block 0
            // put initial state into block 0, 0 is the initial state
            BlockID initial_block = L[0].blid;
            std::swap(P[0], P[initial_block]);
            fix_blid(0);
            fix_blid(initial_block);
        }
#if 1 || defined(TRACE_AUTOMATA)
		printMappings();
        printf("refine completed, iterations=%ld, split[num=%ld, non=%ld], size[org=%ld, min=%ld]\n"
              , iterations, num_split, non_split, (long)L.size(), (long)P.size());
#endif
	}
//############################################################################

