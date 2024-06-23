// gen_c_algo_code.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdlib.h>

void print_swap(FILE* fp, int idx, const char* szType)
{
	fprintf(fp, "{ register %s t=((%s*)(x))[%d];((%s*)(x))[%d]=((%s*)(y))[%d];((%s*)(y))[%d]=t; }\\\n"
		, szType
		, szType, idx
		, szType, idx
		, szType, idx
		, szType, idx
		);
}

int main(int argc, char* argv[])
{
	FILE* fp = fopen("elem_size_loop.h", "w+");
	if (NULL == fp)
	{
		printf("can not open elem_size_loop.h for write\n");
		return 1;
	}
	int max_elem_size = argc >= 2 ? atoi(argv[1]) : 256;

	fprintf(fp,
		"// input macro:\n"
		"//   ELEM_SIZE_FILE_NAME\n"
		"\n"
		"#define VALUE_SIZE 1\n"
		"#define FAST_ELEM_SIZE 1\n"
		"#include ELEM_SIZE_FILE_NAME\n"
		"#undef VALUE_SIZE\n"
		"#undef FAST_ELEM_SIZE\n"
		"\n"
		"#define VALUE_SIZE 2\n"
		"#define FAST_ELEM_SIZE 2\n"
		"#include ELEM_SIZE_FILE_NAME\n"
		"#undef VALUE_SIZE\n"
		"#undef FAST_ELEM_SIZE\n"
		"\n"
		"#define VALUE_SIZE 4\n"
		"#define FAST_ELEM_SIZE 4\n"
		"#include ELEM_SIZE_FILE_NAME\n"
		"#undef VALUE_SIZE\n"
		"#undef FAST_ELEM_SIZE\n"
		"\n"
	);

	for (int elem = 8; elem <= max_elem_size; elem += 4)
	{
		fprintf(fp, "#if TERARK_C_MAX_VALUE_SIZE >= %d && %d %% TERARK_C_STRUCT_ALIGN == 0\n", elem, elem);
		fprintf(fp, "#define VALUE_SIZE %d\n", elem);
		fprintf(fp, "#define FAST_ELEM_SIZE %d\n", elem);
		fprintf(fp, "#include ELEM_SIZE_FILE_NAME\n");
		fprintf(fp, "#undef VALUE_SIZE\n");
		fprintf(fp, "#undef FAST_ELEM_SIZE\n");
		fprintf(fp, "#endif\n\n");
	}
	fclose(fp);

	fp = fopen("swap64.h", "w+");
	if (NULL == fp)
	{
		printf("can not open swap64.h for write\n");
		return 1;
	}
	fprintf(fp, "// this file is auto generated, DO NOT EDIT it\n\n");
	for (int elem = 8; elem <= max_elem_size; elem += 8)
	{
		fprintf(fp, "#define Value_swap_%d(x,y) do {\\\n", elem);
		for (int j = 0; j < elem/8; ++j)
			print_swap(fp, j, "BK8");
		fprintf(fp, " } while(0) \n");
	}
	fprintf(fp, "\n");
	for (int elem = 4; elem <= max_elem_size; elem += 8)
	{
		fprintf(fp, "#define Value_swap_%d(x,y) do {\\\n", elem);
		if (elem >= 8)
		{
			for (int j = 0; j < elem/8; ++j)
				print_swap(fp, j, "BK8");
		}
		print_swap(fp, elem/4 - 1, "int");
		fprintf(fp, " } while(0) \n\n");
	}
	fclose(fp);
	fp = fopen("swap32.h", "w+");
	if (NULL == fp)
	{
		printf("can not open swap32.h for write\n");
		return 1;
	}
	fprintf(fp, "// this file is auto generated, DO NOT EDIT it\n\n");
	for (int elem = 4; elem <= max_elem_size; elem += 4)
	{
		fprintf(fp, "#define Value_swap_%d(x,y) do {\\\n", elem);
		for (int j = 0; j < elem/4; ++j)
			print_swap(fp, j, "int");
		fprintf(fp, " } while(0) \n\n");
	}
	fclose(fp);
	return 0;
}

