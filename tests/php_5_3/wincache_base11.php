<?php
include 'common.inc';
logToFile('Testing ?: expression:');
$a=123;
$b=$a?:456;
myecho($b);

?>
