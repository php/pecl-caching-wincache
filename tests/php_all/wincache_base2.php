<?php
include "common.inc";
logToFile("Testing Function call in case insensitive way: ");
function simple()
{
    myecho("Hello World\r\n");
}
Simple();
SIMPLE();
?>