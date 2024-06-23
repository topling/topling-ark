<?php
	$map = array();
	$map[0] = "0000";
	$map[1] = 314159265358;
	$map[2] = 271828;
	$map[3] = 618;
	$map[4] = array("pqr", "xyz");
	$map["abc"] = "ABC";
	echo serialize($map);
?>

