#!/usr/bin/ccod
<?csp
	struct entry_type
	{
    	char name[110];
		char message[510];
		time_t date;
	};

	char *form_name = "";
	char *form_message = "";
	int empty_entry = 0;
	FILE *f;


	f = fopen("messages.dat", "rb");
	if (!f)
	{
		f = fopen("messages.dat", "wb+");
		if (!f)
		{
			Response.End();
		}
	}
	fclose(f);
	
	char *strtrim(char *str)
	{
		char *nstr = (char*)malloc(strlen(str)+1);
		char *p;
	
		p = str;
		while (*p&&isspace(*p)) p++;
		strcpy(nstr, p);
		p = nstr+strlen(nstr)-1;
		while (isspace(*p)&&p>nstr)
		{
			*p = 0;
			p--;
		}
		return nstr;
	} 
	
	void nomessages()
	{
		?><div id="nomessages">There are currently no entries.</div><?
	}

	
	void add_entry()
	{
		form_name = strtrim(Request.Form.Get("name"));
		form_message = strtrim(Request.Form.Get("message"));
		if (!*form_message||!*form_name)
		{
			empty_entry = 1;
		}
		else
		{
			struct entry_type entry;
			strncpy(entry.name, form_name, 100);
			strncpy(entry.message, form_message, 100);
			entry.date = time(0);

   			f = fopen("messages.dat", "rb+");
			
			fseek(f, 0, SEEK_END);
			fwrite(&entry, 1, sizeof(struct entry_type), f);
			rewind(f);
			
			form_name = "";
			form_message = "";
			fclose(f);
			Response.Redirect("cspgb.c");
		}
	}

?>
<html>
	<head>
		<title>CSP Guestbook</title>
		<style>
			<!--
			body, table, tr, td { background-color: #eeeeee; font-family: verdana, arial, sans; font-size: 10pt; color: #333333; }
			hr { background-color: #333333; width: 550px; }
			#title { text-align: center; font-size: 12pt; font-weight: bold; align: center; }
			.message { width: 550px; }
			.message #date { font-size: 8pt; color: #666666; font-style: italic; }
			.message #name { font-size: 9pt; color: #666666; }
			.message { padding: 0px; margin-top: 5px; }
			.message #message {	border: 1px solid silver; background-color: #dddddd; color: #666666; }
			.messages-all {	text-align: center;	width: 100%; border: 1px solid black; }
			#new-message #name { width: 200px }
			#new-message #message { width: 200px; height: 75px }
			-->
		</style>
	</head>
	<body>
		<div align="center">
		<div id="title">CSP Guestbook</div>
		<hr/>
		<?csp {
			if (!strcmp(Request.Form.Get("submit"), "Submit"))
			{
				add_entry();
			}
			if (empty_entry)
			{
				?><div style="color:red"><i>The name and message are required.</i></div><?
			}
		}?>
		<?csp {
			f = fopen("messages.dat", "rb");
			if (!f)
			{
		    	nomessages(); 	
			}
			else
			{
				int s;
				fseek(f, 0, SEEK_END);
				s = ftell(f);
				rewind(f);
				if (s==0)
				{
					nomessages();
				}
				else
				{
					struct entry_type entry;
				  	while (fread(&entry, 1, sizeof(struct entry_type), f))
					{
						?><table class="message"><?
						?><tr><td id="name">Posted by: <?Response.Write("%s", Server.HTMLEncode(entry.name))?></td><td id="date" align="right">Posted: <?Response.Write("%s", asctime(localtime(&(entry.date))))?></td></tr><?
						?><tr><td id="message" colspan="2"><?Response.Write("%s", Server.HTMLEncode(entry.message))?></td></tr><?							
						?></table><?
					}
				}
			}
			fclose(f);
		}?>

		<hr/>
		<form method="POST">
		<div id="title">Enter a Guestbook Entry</div>
		<hr/>
		<table id="new-message">
			<tr>
				<td>Name</td>
				<td><input type="text" name="name" id="name" value="<?Response.Write("%s", Server.HTMLEncode(form_name))?>" maxlength="100"/></td>
			</tr>
			<tr>
				<td>Message</td>
				<td><textarea name="message" id="message" maxlength="500"><?Response.Write("%s", Server.HTMLEncode(form_message))?></textarea></td>
			</tr>
			<tr>
				<td colspan="2" align="center"><input type="submit" name="submit" id="submit" value="Submit"/></td>
			</tr>
		</table>
		</form>

		<hr>
		<center>
		<i>This is an example guestbook written in C/CSP.  Released to the public domain.</i>
		</center>
		</div>
	</body>
</html>

