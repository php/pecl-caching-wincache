<?php
include 'common.inc';
logToFile("Testing static variables inside class: ");
class MyParent {
    
    protected static $variable;
}

class Child1 extends MyParent {
    
    function set() {
        
        self::$variable = 2;
    }
}

class Child2 extends MyParent {
    
    function show() {
        
        myecho(self::$variable);
    }
}

$c1 = new Child1();
$c1->set();
$c2 = new Child2();
$c2->show(); // prints 2
?> 