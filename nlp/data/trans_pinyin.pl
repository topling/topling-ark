#!/usr/bin/perl -w

while (<>) {
	chomp;
	s/ā/a1/g;
	s/á/a2/g;
	s/ǎ/a3/g;
	s/à/a4/g;
	s/ē/e1/g;
	s/é/e2/g;
	s/ě/e3/g;
	s/ě/e3/g;
	s/è/e4/g;
	s/ī/i1/g;
	s/í/i2/g;
	s/ǐ/i3/g;
	s/ì/i4/g;
	s/ń/en2/g;
	s/ň/en3/g;
	s/ǹ/en4/g;
	s/ō/o1/g;
	s/ó/o2/g;
	s/ǒ/o3/g;
	s/ò/o4/g;
	s/ū/u1/g;
	s/ú/u2/g;
	s/ǔ/u3/g;
	s/ù/u4/g;
	s/ü/v/g;
	s/ǖ/v1/g;
	s/ǘ/v2/g;
	s/ǚ/v3/g;
	s/ǜ/v4/g;
	@F = split " ";
	if ($F[1] =~ /[0-5]/) {
		my $yindiao = $&;
	   	$F[1] =~ s/[0-5]//;
		printf("%s\t%s%d\n", $F[0], $F[1], $yindiao);
	} else {
		printf("%s\t%s\n", $F[0], $F[1]);
	}
}

