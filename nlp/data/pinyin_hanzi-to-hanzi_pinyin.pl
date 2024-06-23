#!/usr/bin/perl -w

use Encode;

while (<>) {
	chomp;
	my @F = split " ";
	if (@F >= 2) {
		my $hanzi_list = decode("UTF-8", $F[1]);
	#	printf("length(%s)=%d\n", $_, length); 
		for(my $i = 0; $i < length($hanzi_list); $i++) {
			printf("%s\t%s\n", encode('UTF-8', substr($hanzi_list,$i,1)), $F[0]);
		}
	} else {
		#printf STDERR "ERROR: pinyin has no hanzi: %s\n", $_;
	}
}

