<?php
$inc = 'wincache_base3.php';
include_once($inc);
use my\name\myclass As Another;
$obj = new Another;
$obj->myfunction();
my\name\myfunction();
?>