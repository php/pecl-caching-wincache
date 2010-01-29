<?php
include "common.inc";
logToFile("Testing overload of method __get: ");
class c {
  public $p ;
  public function __get($name) { return "__get of $name" ; }
}

$c = new c ;
myecho($c->p . "\r\n");    // declared public member value is empty
$c->p = 5 ;
myecho($c->p . "\r\n");     // declared public member value is 5
unset($c->p) ;
myecho($c->p . "\r\n");     // after unset, value is "__get of p"
?> 
