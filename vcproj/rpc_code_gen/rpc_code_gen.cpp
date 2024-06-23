// rpc_code_gen.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

void gen_arglist_ref_n(FILE* fp, int nParams)
{
	int i;
	fprintf(fp, "template<class ThisType");
	for(i = 0; i < nParams; ++i)
		fprintf(fp, ", class Arg%d", i);
	fprintf(fp, ">\n");
	fprintf(fp, "class arglist_ref<rpc_ret_t (ThisType::*)(");
	for(i = 0; i < nParams; ++i)
	{
		fprintf(fp, "Arg%d", i);
		if (i != nParams-1)
			fprintf(fp, ", ");
	}
	fprintf(fp, ")>\n");
	fprintf(fp, "	ThisType* self;\n");
}

void gen_server_stub_n(FILE* fp, int nParams)
{
	int i;
	fprintf(fp, "template<class ThisType");
	for(i = 0; i < nParams; ++i)
		fprintf(fp, ", class Arg%d", i);
	fprintf(fp, ">\n");
	fprintf(fp, "class s_arg_packet<rpc_ret_t (ThisType::*)(");
	for(i = 0; i < nParams; ++i)
	{
		fprintf(fp, "Arg%d", i);
		if (i != nParams-1)
			fprintf(fp, ", ");
	}
	fprintf(fp, ")>\n");
	fprintf(fp, "	: public s_arg_packet_base\n");
	fprintf(fp, "{\n");
	fprintf(fp, "public:\n");
	fprintf(fp, "	typedef rpc_ret_t (ThisType::*function_t)(");
	for(i = 0; i < nParams; ++i)
	{
		fprintf(fp, "Arg%d", i);
		if (i != nParams-1)
			fprintf(fp, ", ");
	}
	fprintf(fp, ");\n");

	fprintf(fp, "	typedef arglist_val<function_t> val_t;\n");
	fprintf(fp, "	typedef arglist_ref<function_t> ref_t;\n");
	fprintf(fp, "	aval_t argvals;\n");

	fprintf(fp, "	void invoke_f(function_t fun)\n");
	fprintf(fp, "	{\n");
	fprintf(fp, "		this->retv = (argvals.self->*fun)(");
	for(i = 0; i < nParams; ++i)
	{
		fprintf(fp, "\n\t\t\tArgVal<Arg%d>::deref(argvals.a%d)", i, i);
		if (i != nParams-1)
			fprintf(fp, ",");
	}
	fprintf(fp, "\n\t\t);\n");
	fprintf(fp, "	}\n");
	fprintf(fp, "};\n");
	fprintf(fp, "\n");
}

void gen_stub(int nMaxParams, const char* fname,
			  void (*gen_n)(FILE* fp, int nParams))
{
	FILE* fp = fopen(fname, "w");
	if (0 == fp)
		exit(1);
	int i;
	for (i = 0; i <= nMaxParams; ++i)
	{
		gen_n(fp, i);
	}
	fclose(fp);
}

int main(int argc, char* argv[])
{
	gen_stub(20, "server_stub.inl", gen_server_stub_n);

	return 0;
}

