#include <stdio.h>
#include <ctype.h>

int main() {
	printf(" = { // tolower\n");
	for (int ch = 0; ch < 256; ) {
		for (int i = 0; i < 16; ++i, ++ch) {
			printf(" 0x%02X,", tolower(ch));
		}
		printf("\n");
	}
	printf("};\n\n");

	printf(" = { // toupper\n");
	for (int ch = 0; ch < 256; ) {
		for (int i = 0; i < 16; ++i, ++ch) {
			printf(" 0x%02X,", toupper(ch));
		}
		printf("\n");
	}
	printf("};\n\n");
	return 0;
}

