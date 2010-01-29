<?php

class CallStatic
{
        static public function __callStatic ($name, $args)
        {
                return self::obj((object) $args[0]);
        }

        static public function obj (stdClass $object)
        {
                return $object;
        }
}
?>