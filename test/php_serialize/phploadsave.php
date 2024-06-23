<?php
	$data = file_get_contents('php://stdin');
	$obj = unserialize($data);
	echo serialize($obj);
?>

