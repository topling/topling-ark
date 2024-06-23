<?php
	$root = array(1, -2, 3, -4, 5, -6, "abc", "123");
	$root[] = array(1234);
	$a = &$root;
	for ($i = 0; $i < 1000; $i++) {
		for ($j = 0; $j < 100; $j++) {
			$a[count($a)] = $i*$j;
		}
		$a[count($a)] = array($i);
		$a = &$a[count($a)-1];
	}
	echo serialize($root);
?>

