<?php
include 'common.inc';
logToFile('Testing GOTO statement: ');
for($i=0,$j=50; $i<100; $i++) {
  while($j--) {
    if($j==17) goto end; 
  }  
}
myecho("i = $i");
end:
myecho('j hit 17');
?>
