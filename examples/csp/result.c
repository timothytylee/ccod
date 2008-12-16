#!/usr/bin/ccod

<!-- File: result.c -->

<?
	Response.Write("Field 1: %s<br><br>", Request.Form.Get("field_1"));
	Response.Write("<b>All Fields Dumped</b><br>");
	for (int i=0;i<Request.Form.Count;i++)
	{
		Response.Write("%s: %s<br>", Request.Form.Key(i), Request.Form.Value(i));
	}
?>
