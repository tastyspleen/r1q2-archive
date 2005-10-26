<?php
	if (isset($_FILES['report']))
	{
		$username = basename($_FILES['report']['name']);
		$length = strlen($username) - 4;

		$fname = "/home/r1ch/r1ch.net/public_html/stuff/r1q2/.crashdumps/";

		for ($i = 0; $i < $length; $i++)
		{
			if (preg_match ("/[A-Za-z0-9-_]/", $username[$i]))
			{
				$fname .= $username[$i];
			}
		}

		$fname .= sha1 (time() . $_SERVER['REMOTE_ADDR']);

		move_uploaded_file ($_FILES['report']['tmp_name'], "{$fname}.txt");

		if (isset($_FILES['minidump']))
		{
			move_uploaded_file ($_FILES['minidump']['tmp_name'], "{$fname}.dmp");
		}

		print "Upload accepted.";
		exit;
	}
?>
<h1>There Be Dragons Here</h1>
