#!/usr/bin/ccod
<?
/*
Gotos (gotos.c)

The forbidden fruit.
*/
goto l__movefar;

l__moveclose:

printf("Now I am here.\n");
return 0;
?>
This is some inline text that will never be reached!
<?
l__movefar:

printf("Here I am.\n");
goto l__moveclose;

?>
