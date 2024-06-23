#!/usr/bin/perl -w

use Encode;

my %DuoYinZi = ();
my @words = ();

my $DuoYinZiFile;
#my $DuoYinZiFname = "norm/cjk-duoyin-none-yindiao.txt";
my $DuoYinZiFname = "norm/good-gbk-pinyin-no-yindiao.txt";
#my $DuoYinZiFname = "../../tools/automata/basepinyin.txt";
if (!open $DuoYinZiFile, "<", $DuoYinZiFname) {
	die "failed open file '$DuoYinZiFname', $!";
}
while (<$DuoYinZiFile>) {
	chomp;
	my @F = split " ";
	for (my $i = 1; $i < @F; ++$i) {
		push @{$DuoYinZi{$F[0]}}, $F[$i];
	}
}
close($DuoYinZiFile);

#while (my ($key, $val) = each %DuoYinZi) {
#	printf("%s %d %s\n", $key, scalar(@$val), join(" ", @$val));
#} exit;

my $MinFaYinNum;
if (@ARGV >= 1) {
	$MinFaYinNum = pop(@ARGV);
} else {
	printf STDERR "usage: %s [word-files] MinFaYinNum\n  If word-files is not specified, read from stdin\n", $0;
	exit 1;
}

while (<>) {
	chomp;
	s/[\x{00}-\x{7F}]//g;
#	print "$_\n";
#	now $_ has only non-ascii chars
	my $FaYinNum = 1;
	my @FaYinAll = ();
	$_ = decode("UTF-8", $_);
#	printf("length(%s)=%d\n", $_, length); 
	for(my $i = 0; $i < length; $i++) {
		my $ThisFaYin = $DuoYinZi{encode("UTF-8", substr($_, $i, 1))};
		if ($ThisFaYin) {
			$FaYinNum = $FaYinNum * @$ThisFaYin;
			$FaYinAll[$i] = @$ThisFaYin;
		} else {
			$FaYinAll[$i] = 1;
		}
	}
	if ($FaYinNum >= $MinFaYinNum) {
		printf("%4d ", $FaYinNum);
		for (my $i = 0; $i < length; ++$i) {
			printf("%X", $FaYinAll[$i]);
		}
		printf("%.*s", 15-length, " " x 100);
		printf(" %s\n", encode("UTF-8", $_));
	}
}

