<?php
class MyClassFoo
{
	protected $E;
	protected $PI;

	public function __construct() {
		$E  = 271828;
		$PI = 3141526;
	}

	function foo()
	{
		echo "Doing foo."; 
	}
}
	$list = array("abcdef", "leipeng", -5678, TRUE, FALSE, array("ABC", "123", 8, 9, NULL), array(new MyClassFoo));
	echo serialize($list);
?>

