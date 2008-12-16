#!/usr/bin/ccod
<?
	Response.Write("This is text that will never appear ");
	Response.Write("because the buffer that holds it is cleared.<br>");

	Response.Clear();

	Response.Write("Now this text is real!.<br>");
?>
