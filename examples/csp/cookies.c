#!/usr/bin/ccod
<?
	Response.Write("Var1 : [%s]<br>", Request.Cookies.Get("Var1"));
	Response.Cookies.Set("Var1", "MyVal");
	Response.Write("Var1 : [%s]<br>", Request.Cookies.Get("Var1"));
	Response.Cookies.Set("Var1", "");
	Response.Write("Var1 : [%s]<br>", Request.Cookies.Get("Var1"));
?>
