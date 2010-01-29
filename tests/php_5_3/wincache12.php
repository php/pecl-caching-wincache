<?php
require_once 'common.inc';
logToFile("Testing late binding: ");
class One {
	protected static $var;

	public static function getVar() {
		return static::$var;
	}
}

class Two extends One {
	public static $var = 'two';
}

myecho(Two::getVar()."\r\n");
?>
