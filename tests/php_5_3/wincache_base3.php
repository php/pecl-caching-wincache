<?php
namespace my\name;
include "common.inc";
logToFile("Testing namespace: ");
class MyClass {function myfunction() { myecho("Hello world\r\n");}}
function myfunction() { myecho("Hello world\r\n");}
const MYCONST = 1;

$a = new MyClass;
$a->myfunction();
$c = new \my\name\MyClass;
$c->myfunction();
?> 