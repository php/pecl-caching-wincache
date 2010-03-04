<?php
function foo()
{
    global $color;
    if ((require 'wincache_base4.php') == 1)
        myecho("A $color $fruit\r\n");
}

foo();                    // A green apple
myecho("A $color $fruit\r\n");   // A green
?> 