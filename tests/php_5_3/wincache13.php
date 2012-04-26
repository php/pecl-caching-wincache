<?php
include 'common.inc';
logToFile('Testing anonymous function: ');
$f = function(){
	myecho("Hello World!\r\n");
};
$f();
?>