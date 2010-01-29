<?php
require_once "common.inc";
logToFile("Testing Global Variables: ");
$variable = 2.0;
function testGlobal()
{
    global $variable;
    myecho("$variable\r\n");
}
testGlobal();
$variable += 1;
testGlobal();
$variable = "Changing to string.";
testGlobal();
?>