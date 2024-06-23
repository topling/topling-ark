#!/usr/bin/perl -w

use Encode;

while (<>) {
	chomp;
	if (m/^([a-zA-Z0-9]+)\s*=>\s*'?([^']*).*/) {
		print("$1\t$2\n");
	} else {
		print STDERR "ERROR: ", $_;
	}
}

