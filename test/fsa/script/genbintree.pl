#!/usr/bin/perl
if (@ARGV < 2) {
	die "usage: $0 max_bin_num width";
}
my $width = $ARGV[1];
for ($i = 0; $i < $ARGV[0]; ++$i) {
	printf "%0${width}b\n", $i
}


