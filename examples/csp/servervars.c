#!/usr/bin/ccod
<table border=1>
<?
	for (int i=0;i<Request.ServerVariables.Count;i++)
	{
		?><tr><?
		Response.Write("<td>%s</td>", Request.ServerVariables.Key(i));
		Response.Write("<td>%s</td>", Request.ServerVariables.Value(i));
		?></tr><?
	}
?>
</table>
