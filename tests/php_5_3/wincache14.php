<?php
include_once 'common.inc';
logToFile('Testing Closure: ');
$a = "Hello, Closures!";
$b = function() use ($a) {
    myecho("$a\r\n");
};
$b();
?>
