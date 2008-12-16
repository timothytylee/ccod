#!/usr/bin/ccod
<?
	Response.Write("Var1 : [%s]<br>", Session.Get("Var1"));
	Session.Set("Var1", "MyVal");
	Response.Write("Var1 : [%s]<br>", Session.Get("Var1"));
	Session.Abandon();
	Response.Write("Var1 : [%s]<br>", Session.Get("Var1"));
?>
