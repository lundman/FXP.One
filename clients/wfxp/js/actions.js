// all functions that do something, but not necessarily a daemon command
function doConnect() {
    wsUri = document.getElementById("host").value;
    socket = new WebSocket(wsUri);
    socket.onopen = function(evt) { onOpen(evt) };
    socket.onclose = function(evt) { onClose(evt) };
    socket.onmessage = function(evt) { onMessage(evt) };
    socket.onerror = function(evt) { onError(evt) };

    document.getElementById("connectstatus").innerHTML = "Connecting...";

}
function doClose(){
    if(socket){
        socket.close();
    }
    queue.qid = -1;
    lsite.session = -1;
    rsite.session = -1;
    clearTable('left');
    clearTable('right');
    clearTable('queue');
    toggleView("none");
}
function doAuth() {
    myuser = document.getElementById("user").value;
    mypass = document.getElementById("pass").value;
    if (mypass == "admin")
        alert("\nYou are still using the default password.\nPlease change it as soon as possible.");
    socket.send("AUTH|USER="+myuser+"|PASS="+mypass+"\n")
    document.getElementById("connectstatus").innerHTML = "Authenticating...";
    WriteLog("Authenticating...");
}
 function doSend() {
    var mbox = document.getElementById("send");
    WriteLog("SENT: " + mbox.value);
    socket.send(mbox.value+"\n");
    mbox.value="";
 }
function doEnterConn(){
   if(document.activeElement.id == "pass"){
      doDisConn();
   }
}
function doDisConn(){
   var butt = document.getElementById("connect");
   if(butt.value == "Connect"||butt.value == "Retry"){
      butt.value="Disconnect";butt.alt="Disconnect";butt.title="Disconnect";
      butt.src="img/connected.png";
      doConnect();
   }else{
      butt.value="Connect";butt.alt="Connect";butt.title="Connect";
      butt.src="img/disconnected.png";
      doClose();
   }
}
function doQueueWipe(){
     // nothing here yet
}

function doCWDEdit(side){
    if(connected){
        var site = (side=="left")?lsite:rsite;
        if(site.session <= 0){
            WriteLog("Site "+sitelist[site.siteid]["NAME"]+" not connected!");
        } else {
            var pd = document.getElementById(side.substr(0,1)+"path");
            WriteLog("CWD Change " + side + ": " + pd.value);
            var enc = encode(pd.value);
            socket.send("CWD|SID="+site.session+"|PATH="+enc+"\n");
        }
    }else{
        WriteLog("Not connected!");
    }
}


function doQueue(side){
    if(connected){
        var site = (side=="left")?lsite:rsite;
        // We need to have a QID to be able to queue.
        if((queue.qid == -1) || (site.session == -1)){
            WriteLog("No active queue (Are both sides connected?)");
        } else {
            var pd = document.getElementById(side);
            var checks = pd.getElementsByTagName('input');
            var i;
            for (i = 0; i < checks.length; i++) {
                if (checks[i].type == 'checkbox' && checks[i].checked == true) {
                    var source = (side=="left")?"NORTH":"SOUTH";
                    var fid = checks[i].name.split('#',2)[1];
                    socket.send("QADD|QID="+queue.qid+"|SRC="+source+"|FID="+fid+"\n");
                }
            }
        }
    }else{
        WriteLog("Not connected!");
    }
}


function setAllCheckboxes(side)
{
    var pd = document.getElementById(side);
    var checks = pd.getElementsByTagName('input');
    for (var i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == false) {

            if (checks[i].name.split('#',2)[1] != -1)
                checks[i].checked = true;
        }
    }
}

function setCheckboxes(side, fids)
{
    var pd = document.getElementById(side);
    var checks = pd.getElementsByTagName('input');
    for (var i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == false) {
            var fid = checks[i].name.split('#',2)[1];
            if (fids.indexOf(fid) >= 0)
                checks[i].checked = true;
        }
    }
}

function clearAllCheckboxes(side)
{
    var pd = document.getElementById(side);
    var checks = pd.getElementsByTagName('input');
    for (var i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            checks[i].checked = false;
        }
    }
}

function copyList(side)
{
    var string = "";
    var pd = document.getElementById(side);
    var checks = pd.getElementsByTagName('input');
    var fid;
    var datestr;
    var name;
    var sitedata = (side=="left")?lsite:rsite;
    var byfids = {};
    var size,user,group;
    for (var i = 0; i < sitedata.listing.length; i++) {
        byfids[sitedata.listing[i]["FID"]] = sitedata.listing[i];
    }

    for (var i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {

            fid = checks[i].name.split('#',2)[1];
            if (fid == -1) continue;

            size  = byfids[fid]["SIZE"];
            size  = Array(12 + 1 - size.length).join(" ") + size;
            user  = byfids[fid]["USER"];
            user  = Array(12 + 1 - user.length).join(" ") + user;
            group = byfids[fid]["GROUP"];
            group = Array(12 + 1 - group.length).join(" ") + group;

            datestr = ("DATE" in byfids[fid]) ? time2str(byfids[fid]["DATE"]) : "";
            name = decode(byfids[fid]["NAME"]);
            string += byfids[fid]["PERM"] + " " +
                user +
                " " +
                group +
                " " + size + " " + datestr + " " + name + "\n";

        }
    }
    //alert(string);
    my_window = window.open("", "PDF", "scrollbars=1;resizable=1;width=600;height=800");
    //my_window = window.open("about:blank", "replace", "");
    //my_window = window.open("data:text/html;charset=utf-8,<HTML><BODY><span id='fun'>test</span></body></html>", "tescticles", "");

    my_window.document.write('<pre>\n'+string+'</pre>');
    //my_window.document.body.innerHTML('<pre>\n'+string+'</pre>');

    my_window.document.close();
    //my_window.focus();
    //var doc=my_window.document.open("text/html");
    //var pd=my_window.getElementById("fun");
    //pd.innerHTML = "replaced test with something";
    //doc.close();
    //my_window.focus();
    //my_window.close();
}


function doCmdChange(side)
{
    if (!connected) return;
    var site = (side=="left")?lsite:rsite;
    var pd = document.getElementById(side+"cmd");
    var input = document.getElementById(side+"cmdinput");
    // check for a saved command
    var savedcommand = pd.options[pd.selectedIndex].getAttribute('savedcmd');
    if (savedcommand) {
        document.getElementById(side+"cmdinput").value = savedcommand;
    }
    switch(pd.value) {
    case 'refresh':
    case 'selectall':
    case 'clearall':
    case 'unlink':
    case 'compare':
    case 'copylist':
        input.disabled = true;
        break;
    case 'filter':
    case 'site':
    case 'mkdir':
    case 'rename':
        input.disabled = false;
        break;
    default:
        break;
    }
}


function defaultSite()
{
    var site = {};

    site["NAME"]         = "";
    site["HOST"]         = "";
    site["PORT"]         = "";
    site["USER"]         = "";
    site["PASS"]         = "";
    site["STARTDIR"]     = "";
    site["SVTYPE"]        = "";
    site["PASSIVE"]      = "2";
    site["FXP_PASSIVE"]  = "2";
    site["CONTROL_TLS"]  = "2";
    site["DATA_TLS"]     = "2";
    site["FXP_TLS"]      = "2";
    site["IMPLICIT_TLS"] = "2";
    site["RESUME"]       = "2";
    site["RESUME_LAST"]  = "2";
    site["PRET"]         = "2";
    site["USE_XDUPE"]    = "2";
    site["FMOVEFIRST"]   = "";
    site["FSKIPLIST"]    = "";
    site["DMOVEFIRST"]   = "";
    site["DSKIPLIST"]    = "";
    site["FSKIPEMPTY"]   = "2";
    site["DSKIPEMPTY"]   = "2";
    site["SVDCMDS"]      = "";
    site["RDIRS"]      = "";

    return site;
}


function getExtraPairs(site)
{
    var str="";

    for (var key in site) {
        switch(key) {
        case 'func':
        case 'SITEID':
        case 'NAME':
        case 'HOST':
        case 'PORT':
        case 'USER':
        case 'PASS':
        case 'STARTDIR':
        case 'SVTYPE':
        case 'PASSIVE':
        case 'FXP_PASSIVE':
        case 'CONTROL_TLS':
        case 'DATA_TLS':
        case 'FXP_TLS':
        case 'IMPLICIT_TLS':
        case 'RESUME':
        case 'RESUME_LAST':
        case 'PRET':
        case 'USE_XDUPE':
        case 'FMOVEFIRST':
        case 'FSKIPLIST':
        case 'DMOVEFIRST':
        case 'DSKIPLIST':
        case 'FSKIPEMPTY':
        case 'DSKIPEMPTY':
        case 'SVDCMDS':
        case 'RDIRS':
            break;
        default:
            str += "|"+key+"="+site[key];
        }
    }
    return str;
}

function doSMSiteUpdate()
{
}

function doSMSiteChange()
{
    // flush saved commands data and menu
    encodedsitecommands = '';
    sitesavedcommands.length = 0;
    while (saved_commands.childNodes.length > 0) {
        saved_commands.removeChild(saved_commands.lastChild);
    }
    document.getElementById("sv_newcmdargs").value = '';

    var smsites = document.getElementById("sm_sites");
    var s = smsites.value; // SITEID
    if (s == '-1') {
        site = defaultSite();
        site["FMOVEFIRST"] = "*.sfv/*.nfo";
        site["DMOVEFIRST"] = "Sample";
        site["FSKIPLIST"]  = "*COMPLETE*/*NUKE*/.message/.rules";
        site["DSKIPLIST"]  = "*COMPLETE*/*NUKE*";
    } else {
        site = sitelist[s];
    }

    document.getElementById("sm_name").value = site["NAME"];
    document.getElementById("sm_host").value = site["HOST"];
    document.getElementById("sm_port").value = site["PORT"];
    document.getElementById("sm_user").value = site["USER"];
    document.getElementById("sm_pass").value = site["PASS"];
    document.getElementById("sm_startdir").value = "STARTDIR" in site ? site["STARTDIR"] : "";
    document.getElementById("sm_svtype").value = site["SVTYPE"];
    document.getElementById("sm_pasv").value = site["PASSIVE"];
    document.getElementById("sm_fxpp").value = site["FXP_PASSIVE"];
    document.getElementById("sm_cssl").value = site["CONTROL_TLS"];
    document.getElementById("sm_dssl").value = site["DATA_TLS"];
    document.getElementById("sm_xssl").value = "FXP_TLS" in site ? site["FXP_TLS"] : "2";
    document.getElementById("sm_implicit").value = "IMPLICIT_TLS" in site ? site["IMPLICIT_TLS"] : "2";
    document.getElementById("sm_resume").value = "RESUME" in site ? site["RESUME"] : "2";
    document.getElementById("sm_reslast").value = "RESUME_LAST" in site ? site["RESUME_LAST"] : "2";
    document.getElementById("sm_pret").value = "PRET" in site ? site["PRET"] : "2";
    document.getElementById("sm_xdupe").value = "USE_XDUPE" in site ? site["USE_XDUPE"] : "2";
    document.getElementById("sm_fmove").value = "FMOVEFIRST" in site ? site["FMOVEFIRST"] : "";
    document.getElementById("sm_fskip").value = "FSKIPLIST" in site ? site["FSKIPLIST"] : "";
    document.getElementById("sm_dmove").value = "DMOVEFIRST" in site ? site["DMOVEFIRST"] : "";
    document.getElementById("sm_dskip").value = "DSKIPLIST" in site ? site["DSKIPLIST"] : "";
    document.getElementById("sm_fempty").value = "FSKIPEMPTY" in site ? site["FSKIPEMPTY"] : "2";
    document.getElementById("sm_dempty").value = "DSKIPEMPTY" in site ? site["DSKIPEMPTY"] : "2";

    s = getExtraPairs(site);
    document.getElementById("sm_extra").value = s.replace(/^\|/, "");


    if (site["HOST"] == "<local>") {
        document.getElementById("sm_local").checked = true;
    } else {
        document.getElementById("sm_local").checked = false;
    }
    var svtype = document.getElementById("sm_svtype").value;
    doLoadSiteCommands(svtype);
}

function smLocalChange()
{
    if (document.getElementById("sm_local").checked == true) {
        document.getElementById("sm_host").value = "<local>";
        document.getElementById("sm_host").disabled = true;
        document.getElementById("sm_port").disabled = true;
        document.getElementById("sm_user").disabled = true;
        document.getElementById("sm_pass").disabled = true;
    } else {
        document.getElementById("sm_host").value = "";
        document.getElementById("sm_pass").disabled = false;
        document.getElementById("sm_user").disabled = false;
        document.getElementById("sm_port").disabled = false;
        document.getElementById("sm_host").disabled = false;
    }
}


function smSaveSite()
{
    var save;
    var name = document.getElementById("sm_name").value;
    var site;
    var plain = defaultSite();

    WriteLog("Saving '"+name+"'.");
    for (var key in sitelist) {
        if (name == sitelist[key]["NAME"]) {
            site = sitelist[key];
            save = "SITEMOD|SITEID="+key;
            break;
        }
    }

    if (site == undefined) {
        site = plain;
        save = "SITEADD";
    }

    if (document.getElementById("sm_local").checked) {
        document.getElementById("sm_host").value = "<local>";
        document.getElementById("sm_port").value = "21";
        document.getElementById("sm_user").value = "NA";
        document.getElementById("sm_pass").value = "NA";
    }

    // current site commands
    encodedsitecommands = b64wrap(sitesavedcommands,"encode");

    if ((val = document.getElementById("sm_name").value) !==
        (("NAME" in site) ? site["NAME"] : plain["NAME"]))
        save += "|NAME="+val;
    if ((val = document.getElementById("sm_host").value) !==
        (("HOST" in site) ? site["HOST"] : plain["HOST"]))
        save += "|HOST="+val;
    if ((val = document.getElementById("sm_port").value) !==
        (("PORT" in site) ? site["PORT"] : plain["PORT"]))
        save += "|PORT="+val;
    if ((val = document.getElementById("sm_user").value) !==
        (("USER" in site) ? site["USER"] : plain["USER"]))
        save += "|USER="+val;
    if ((val = document.getElementById("sm_pass").value) !==
        (("PASS" in site) ? site["PASS"] : plain["PASS"]))
        save += "|PASS="+val;
    if ((val = document.getElementById("sm_svtype").value) !==
        (("SVTYPE" in site) ? site["SVTYPE"] : plain["SVTYPE"]))
        save += "|SVTYPE="+val;
    if ((val = document.getElementById("sm_startdir").value) !==
        (("STARTDIR" in site) ? site["STARTDIR"] : plain["STARTDIR"]))
        save += "|STARTDIR="+val;
    if ((val = document.getElementById("sm_pasv").value) !==
        (("PASSIVE" in site) ? site["PASSIVE"] : plain["PASSIVE"]))
        save += "|PASSIVE="+val;
    if ((val = document.getElementById("sm_fxpp").value) !==
        (("FXP_PASSIVE" in site) ? site["FXP_PASSIVE"] : plain["FXP_PASSIVE"]))
        save += "|FXP_PASSIVE="+val;
    if ((val = document.getElementById("sm_cssl").value) !==
        (("CONTROL_TLS" in site) ? site["CONTROL_TLS"] : plain["CONTROL_TLS"]))
        save += "|CONTROL_TLS="+val;
    if ((val = document.getElementById("sm_dssl").value) !==
        (("DATA_TLS" in site) ? site["DATA_TLS"] : plain["DATA_TLS"]))
        save += "|DATA_TLS="+val;
    if ((val = document.getElementById("sm_xssl").value) !==
        (("FXP_TLS" in site) ? site["FXP_TLS"] : plain["FXP_TLS"]))
        save += "|FXP_TLS="+val;
    if ((val = document.getElementById("sm_implicit").value) !==
        (("IMPLICIT_TLS" in site) ? site["IMPLICIT_TLS"] : plain["IMPLICIT_TLS"]))
        save += "|IMPLICIT_TLS="+val;
    if ((val = document.getElementById("sm_resume").value) !==
        (("RESUME" in site) ? site["RESUME"] : plain["RESUME"]))
        save += "|RESUME="+val;
    if ((val = document.getElementById("sm_reslast").value) !==
        (("RESUME_LAST" in site) ? site["RESUME_LAST"] : plain["RESUME_LAST"]))
        save += "|RESUME_LAST="+val;
    if ((val = document.getElementById("sm_pret").value) !==
        (("PRET" in site) ? site["PRET"] : plain["PRET"]))
        save += "|PRET="+val;
    if ((val = document.getElementById("sm_xdupe").value) !==
        (("USE_XDUPE" in site) ? site["USE_XDUPE"] : plain["USE_XDUPE"]))
        save += "|USE_XDUPE="+val;
    if ((val = document.getElementById("sm_fmove").value) !==
        (("FMOVEFIRST" in site) ? site["FMOVEFIRST"] : plain["FMOVEFIRST"]))
        save += "|FMOVEFIRST="+val;
    if ((val = document.getElementById("sm_fskip").value) !==
        (("FSKIPLIST" in site) ? site["FSKIPLIST"] : plain["FSKIPLIST"]))
        save += "|FSKIPLIST="+val;
    if ((val = document.getElementById("sm_dmove").value) !==
        (("DMOVEFIRST" in site) ? site["DMOVEFIRST"] : plain["DMOVEFIRST"]))
        save += "|DMOVEFIRST="+val;
    if ((val = document.getElementById("sm_dskip").value) !==
        (("DSKIPLIST" in site) ? site["DSKIPLIST"] : plain["DSKIPLIST"]))
        save += "|DSKIPLIST="+val;
    if ((val = document.getElementById("sm_fempty").value) !==
        (("FSKIPEMPTY" in site) ? site["FSKIPEMPTY"] : plain["FSKIPEMPTY"]))
        save += "|FSKIPEMPTY="+val;
    if ((val = document.getElementById("sm_dempty").value) !==
        (("DSKIPEMPTY" in site) ? site["DSKIPEMPTY"] : plain["DSKIPEMPTY"]))
        save += "|DSKIPEMPTY="+val;
    if ((val = encodedsitecommands) !==
        (("SVDCMDS" in site) ? site["SVDCMDS"] : plain["SVDCMDS"]))
        if (typeof val !== 'undefined') {
           save += "|SVDCMDS="+val;
        }
    if ((val = document.getElementById("sm_extra").value) !==
        getExtraPairs(site))
        save += "|"+val;

    // Send SITEADD or SITEMOD
    socket.send(save+"\n");
}

function smDeleteSite()
{
    var smsites = document.getElementById("sm_sites");
    var s = smsites.value; // SITEID
    if (s == '-1') {
        return;
    }
    socket.send("SITEDEL|SITEID="+s+"\n");
}


function doCommand(side)
{
    if (!connected) return;
    var site = (side=="left")?lsite:rsite;
    // if (site.session == -1) return;
    var pd = document.getElementById(side+"cmd");
    var input = document.getElementById(side+"cmdinput");
    switch(pd.value) {
    case 'refresh':
        WriteLog("Refreshing directory");
        site.filter = null;
        do_DIRLIST(site.session);
        break;
    case 'filter':
        WriteLog("Refreshing directory");
        site.filter = input.value;
        do_DIRLIST(site.session);
        break;
    case 'selectall':
        setAllCheckboxes(side);
        break;
    case 'clearall':
        clearAllCheckboxes(side);
        break;
    case 'site':
        do_SITECMD(site.session, input.value);
        break;
    case 'mkdir':
        do_MKDCMD(site.session, input.value);
        break;
    case 'unlink':
        do_UNLINKCMD(site.session,  (side=="left")?"$<":"$>");
        break;
    case 'rename':
        do_RENCMD(site.session, input.value);
        break;
    case 'compare':
        if ((queue.qid == -1) || (lsite.session == -1) || (rsite.session == -1)) {
            WriteLog("Both sites need to be connected for COMPARE");
            return;
        }
        do_COMPARE(queue.qid);
        break;
    case 'copylist':
        copyList(side);
        break;
    default:
        WriteLog("Not yet implemented, jefe");
    }
}



// all functions that execute an action on the engine with corresponding handler
function do_SITELIST(){
   socket.send("SITELIST\n");
}
function do_SESSIONNEW(side){
   if(connected){
      var site = (side=="left")?lsite:rsite;
      if(site.session != -1){
         WriteLog("Site "+sitelist[site.siteid]["NAME"]+" already connected!");
      }else{
         var pd = document.getElementById(side.substr(0,1)+"slist");
         site.siteid = pd.options[pd.selectedIndex].value;

         //put the textbox back
         if (side == "left") {
             var sd = "l";
         }else{ var sd = "r"; }
         var select = document.getElementById(sd+"dirselect");
         select.style.display = 'none';
         var textbox = document.getElementById(sd+"path");
         textbox.style.display = 'inline';
         textbox.value = '';

         // wipe filter from last session
         site.filter = null;

         // new session means new side+siderecentdirs
         eval(side+"siderecentdirs").length=0;

         // get recent dirs, sets siterecentdirs
         svCmdReadWrite("read","RDIRS",site.siteid);

         // populate recent directories
         if (siterecentdirs.length > 1) {
             if (side == "left") {
                 leftsiderecentdirs = siterecentdirs.slice();
             }else if (side == "right") {
                 rightsiderecentdirs = siterecentdirs.slice();
             }
             // flush for next use
             siterecentdirs.length=0;
         }

         // populate top menu saved commands when we (try to) connect
         // read from object into tmp so sitesavedcmds doesn't break
         var tmptopcmdmenu = svCmdReadWrite("read","SVDCMDS",site.siteid);
         if (tmptopcmdmenu.length > 1) {
             var tmptopmenu = svCmdReadWrite("read","SVDCMDS",site.siteid).slice();
             for (var i = 0; i < tmptopmenu.length; i++) {
                 svCmdTopMenu(tmptopmenu,side);
             }
         }

         // LOG=1 needed for LOG command
         socket.send("SESSIONNEW|SITEID="+site.siteid+"|LOG=1\n");
         dirlistStatus(side,"**CONNECTING**");
      }
    }else{
      WriteLog("Not connected!");
   }
}
function do_SESSIONFREE(side)
{
    if(connected){
        var site = (side=="left")?lsite:rsite;
        // if site.session isn't defined we're not connected at all
        if(site.session == -1) {
            WriteLog("Not connected to a site!");
        }else{
            if (queue.qid != -1){
                do_QUEUEFREE(queue.qid);
            }

            //put the textbox back
            if (side == "left") {
                var sd = "l";
            }else{ var sd = "r"; }
            var select = document.getElementById(sd+"dirselect");
            select.style.display = 'none';
            var textbox = document.getElementById(sd+"path");
            textbox.style.display = 'inline';
            textbox.value = '';
            //clear the site cmd box
            var cmdbox = document.getElementById(side+"cmdinput");
            cmdbox.value = '';

            socket.send("SESSIONFREE|SID="+site.session+"\n");
        }
    }else{
        WriteLog("Not connected!");
    }
}
function do_QUEUENEW(){
   if(connected){
      if (queuenewlock == 0) {
          queuenewlock = 1;
          socket.send("QUEUENEW|NORTH_SID="+lsite.session+"|SOUTH_SID="+rsite.session+"\n");
      }else{ return; }
   }
}
function do_QUEUEFREE(qid){
    if(connected){
        socket.send("QUEUEFREE|QID="+qid+"\n");
    }
}
function do_CWD(sid,dir){
    if(connected){
        if(sid == lsite.session){
            side="left";
        }else if(sid == rsite.session){
            side="right";
        }
        socket.send("CWD|SID="+sid+"|PATH="+dir+"\n");
    }
}
function do_PWD(sid){
    if(connected){
        socket.send("PWD|SID="+sid+"\n");
    }
}
function do_DIRLIST(sid){
   if(sid == lsite.session){side="left";}
   else if(sid == rsite.session){side="right";}
   if(connected){
      clearTable(side);
      socket.send("DIRLIST|SID="+sid+"\n");
   }
}

function do_GO() {
    if (queue.qid == -1) return;

    // session is going to drop, save rdirs on both sides
    var sitel = lsite;
    if(sitel.session > 0) {
        svCmdReadWrite("write","RDIRS",sitel.siteid);
    }
    var siter = rsite;
    if(siter.session > 0) {
        svCmdReadWrite("write","RDIRS",siter.siteid);
    }

    socket.send("GO|SUBSCRIBE|QID="+queue.qid+"\n");
}

function do_STOP() {
    if (queue.qid == -1) return;
    socket.send("STOP|QID="+queue.qid+"\n");
}

function do_QITOP() {
    if (queue.qid == -1) return;
    var pd = document.getElementById('queue');
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 1; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            socket.send("QMOVE|QID="+queue.qid+"|FROM="+i+"|TO=FIRST\n");
        }
    }
}
function do_QIBOTTOM() {
    if (queue.qid == -1) return;
    var pd = document.getElementById('queue');
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 0; i < checks.length-1; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            socket.send("QMOVE|QID="+queue.qid+"|FROM="+i+"|TO=LAST\n");
        }
    }
}
function do_QIUP() {
    if (queue.qid == -1) return;
    var pd = document.getElementById('queue');
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 1; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            socket.send("QMOVE|QID="+queue.qid+"|FROM="+i+"|TO="+(i-1)+"\n");
        }
    }
}
function do_QIDOWN() {
    if (queue.qid == -1) return;
    var pd = document.getElementById('queue');
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 0; i < checks.length-1; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            socket.send("QMOVE|QID="+queue.qid+"|FROM="+i+"|TO="+(i+1)+"\n");
        }
    }
}
function do_QIDELETE() {
    if (queue.qid == -1) return;
    var pd = document.getElementById('queue');
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = checks.length-1; i >= 0; i--) { // reverse
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            socket.send("QDEL|QID="+queue.qid+"|@="+i+"\n");
        }
    }
}
function do_QICLEAR() {
    if (queue.qid == -1) return;
    do_QUEUEFREE(queue.qid);
    do_QUEUENEW();
}
function do_QISELECT() {
    var pd = document.getElementById('queue');
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == false) {
            checks[i].checked = true;
        }
    }
}
function do_QIDESELECT() {
    if (queue.qid == -1) return;
    var pd = document.getElementById('queue');
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            checks[i].checked = false;
        }
    }
}
function do_QIINSERT() {
}

// Execute CMD. If CMD contain $< or $> iterate left,right selected items.
function expand_input(cmd, onlyfiles, onlydirs, original)
{
    var reply = new Array();
    var offset;
    var re;
    var side;

    if ((offset = cmd.indexOf("$<")) >= 0) { // iterate left
        side = 'left';
        re = /\$\</gi;
    } else if ((offset = cmd.indexOf("$>")) >= 0) { // iterate right
        side = 'right';
        re = /\$\>/gi;
    } else {
        reply.push(cmd);
        return reply;
    }

    // Iterate side, build array with replacements.
    var pd = document.getElementById(side);
    var checks = pd.getElementsByTagName('input');
    var sitedata = (side=="left")?lsite:rsite;
    var byfids = {};
    for (var i = 0; i < sitedata.listing.length; i++) {
        byfids[sitedata.listing[i]["FID"]] = sitedata.listing[i];
    }
    for (var i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            var fid = checks[i].name.split('#',2)[1];
            if (onlydirs == true) {
                if (byfids[fid]["FTYPE"].toLowerCase() != "directory")
                    continue;
            }
            if (onlyfiles == true) {
                if (byfids[fid]["FTYPE"].toLowerCase() == "directory")
                    continue;
            }
            reply.push(cmd.replace(re, byfids[fid]["NAME"]));
            if (original)
                original.push(byfids[fid]["NAME"]);
        }
    }
    return reply;
}

function do_SITECMD(sid,cmd)
{
    var list = expand_input(cmd);
    for (var i = 0; i < list.length; i++) {
        socket.send("SITE|SID="+sid+"|LOG|CMD="+encode(list[i])+"\n");
    }
}

function do_MKDCMD(sid,cmd)
{
    var list = expand_input(cmd);
    for (var i = 0; i < list.length; i++) {
        socket.send("MKD|SID="+sid+"|PATH="+encode(list[i])+"\n");
    }
    do_DIRLIST(sid);
}

function do_UNLINKCMD(sid,cmd)
{
    // We need to split files and dirs, and delete separately.
    var list = expand_input(cmd, true, false);
    for (var i = 0; i < list.length; i++) {
        socket.send("DELE|SID="+sid+"|PATH="+encode(list[i])+"\n");
    }
    var list = expand_input(cmd, false, true);
    for (var i = 0; i < list.length; i++) {
        socket.send("RMD|SID="+sid+"|PATH="+encode(list[i])+"\n");
    }
    do_DIRLIST(sid);
}

function do_RENCMD(sid,cmd)
{
    var original = new Array();
    var list;

    // Special case, if no pattern is specified, we allow direct rename from
    // one selected item (first selected) to literal input string.
    if ((cmd.indexOf("$<") == -1) &&
        (cmd.indexOf("$>") == -1)) {
        list = expand_input((sid==lsite.session)?"$<":"$>", false, false, original);
        list.length = 1; // First one only
        original.length = 1;
        original[0] = list[0];
        list[0] = cmd; // Remove the "expanded" name
    } else {
        list = expand_input(cmd, false, false, original);
    }

    for (var i = 0; i < list.length; i++) {
        socket.send("REN|SID="+sid+"|FROM="+encode(original[i])+"|TO="+encode(list[i])+"\n");
    }
    do_DIRLIST(sid);
}

function do_COMPARE(qid)
{
    socket.send("QCOMPARE|QID="+qid+"\n");
}




// *** QueueManager

function do_QMRefresh()
{
    socket.send("QLIST\n");
}

function do_QMClearIdle()
{
    for (var i = 0; i < queuemanager.listing.length; i++) {
        if ((queuemanager.listing[i]["STATUS"] == "IDLE") &&
            (queuemanager.listing[i]["QID"] != queue.qid))
            socket.send("QUEUEFREE|QID="+queuemanager.listing[i]["QID"]+"\n");
    }
    do_QMRefresh();
}

function do_QMNuke()
{
    var pd = document.getElementById("qmqueue");
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            var qid = checks[i].name.split('#',2)[1];
            if (qid == queue.qid)
                WriteLog("Releasing this UI's queue will make bad things happen.");
            socket.send("QUEUEFREE|QID="+qid+"\n");
        }
    }
    do_QMRefresh();
}

function do_QMClone()
{
    var pd = document.getElementById("qmqueue");
    var checks = pd.getElementsByTagName('input');
    var i;
    for (i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            var qid = checks[i].name.split('#',2)[1];
            socket.send("QCLONE|QID="+qid+"\n");
        }
    }
    do_QMRefresh();
}

function do_QMSubscribe()
{
    var pd = document.getElementById("qmqueue");
    var checks = pd.getElementsByTagName('input');
    var i;
    var sent=0;
    for (i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            var qid = checks[i].name.split('#',2)[1];
            socket.send("SUBSCRIBE|TOGGLE|QID="+qid+"\n");
            sent++;
        }
    }    
    if (sent)
        do_QMRefresh();
}

function do_QMGrab()
{
    var pd = document.getElementById("qmqueue");
    var checks = pd.getElementsByTagName('input');
    var i;
    var sent=0;
    for (i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            var qid = checks[i].name.split('#',2)[1];
            // GRAB queue, and stop. We only grab the first ticked queue
            socket.send("QGRAB|QID="+qid+"\n");
            return;
        }
    }

}

function do_QGET(qid)
{
    socket.send("QGET|QID="+qid+"\n");
}


function do_QMView()
{
    var pd = document.getElementById("qmqueue");
    var checks = pd.getElementsByTagName('input');
    var i;
    var sent=0;
    for (i = 0; i < checks.length; i++) {
        if (checks[i].type == 'checkbox' && checks[i].checked == true) {
            var qid = checks[i].name.split('#',2)[1];
            socket.send("QERR|QID="+qid+"\n");
            sent++;
        }
    }    
    if (sent)
        do_QMRefresh();
}

function doEnterKey()
{
    var focusbaboon = document.activeElement.id;
    event.preventDefault();
    if (event.keyCode === 13) {
        switch(focusbaboon) {
            case 'leftcmdinput':
                document.getElementById("leftcommand").click();
                break;
            case 'rightcmdinput':
                document.getElementById("rightcommand").click();
                break;
        }
    }
}  

function do_LogClear(clickside)
{
    if (clickside == "left") {
        document.getElementById("lwalloutput").innerHTML = '';
    } else if (clickside == "right") {
        document.getElementById("rwalloutput").innerHTML = '';
    }
}

// all the code for the saved commands editor
function doLoadSiteCommands(svtype)
{
    // reset warning messages
    document.getElementById("savedcmdeditoroutput").innerHTML = '';

    // Standard commands from site cmd menu
    var specialcmds = "";
    // only commands that take arguments
    var standardcmds = ["filter","site cmd","new dir","delete","rename"];

    // FXPOne site commands via 'help'
    var fxponecmds = ["-FXPOne Commands-","adduser","chgrp","chown","colour","deluser","dupe","extra","give","groupadd",
    "groupdel","groupinfo","grouplist","groupuser","gtagline","gtopdn","gtopup","help","incompletes",
    "kick","move","msg","new","nuke","passwd","pre","races","rehash","renuser","reqfilled","request",
    "search","section","setcred","setflags","setip","setlimit","setpass","setratio","tagline","tcpstat",
    "topdn","topup","unnuke","user","wall","who"];

    // add the other more inferior ftp server types here later
    if (svtype == "fxpone") {
        specialcmds = fxponecmds;
    }

    // clear menu first
    while (sv_newcmd.childNodes.length > 0) {
        sv_newcmd.removeChild(sv_newcmd.lastChild);
    }
    // add defaults
    for (var i = 0; i < standardcmds.length; i++) {
        var option = document.createElement("option");
        option.value = standardcmds[i];
        option.text = standardcmds[i];
        if (option.value == "site cmd") {
            option.value = "site";
        } else if (option.text == "site cmd") {
            option.text = "site";
        }
        sv_newcmd.appendChild(option);
    }
    // now the 'specials"
    for (var i = 0; i < specialcmds.length; i++) {
        var option = document.createElement("option");
        option.value = "site "+specialcmds[i];
        option.text = specialcmds[i];
        if (option.value == "site -FXPOne Commands-") {
            option.disabled = true;
            option.text = "-FXPOne Commands-";
        }
    sv_newcmd.appendChild(option);
    }

    // check SVDCMDS value for commands
    var savedCmdsData = site["SVDCMDS"];
    if (savedCmdsData && savedCmdsData.length > 0 ) {
        sitesavedcommands = svCmdReadWrite("read","SVDCMDS",site["SITEID"]).slice();
        svRedrawCmdGUI();
    }
}

function svNewCommand()
{
    // pick a site first, friend
    document.getElementById("savedcmdeditoroutput").innerHTML = '';
    if (document.getElementById("sm_sites").value == "-1") {
        document.getElementById("savedcmdeditoroutput").innerHTML = "Please create or select a site first.";
        return;
    }
    document.getElementById("savedcmdeditoroutput").innerHTML = '';
    var newWriteCmd;
    var newCommand = document.getElementById("sv_newcmd").value;
    var newArgs = document.getElementById("sv_newcmdargs").value.trim();
    //check for empty values
    if (!newCommand) {
       document.getElementById("savedcmdeditoroutput").innerHTML = "Missing command. Please try again.";
       return;
    }
    // empty arguments
    if (!newArgs.replace(/\s/g, '').length) {
        newWriteCmd = newCommand;
    }else{
        newWriteCmd = newCommand.concat(" ",newArgs);
    }
    // check for existing command
    var cmdMatch = sitesavedcommands.indexOf(newWriteCmd);
    if (cmdMatch != -1) {
        document.getElementById("savedcmdeditoroutput").innerHTML = "Command already exists. Please try again.";
        return;
    }else{
        // new command, push to sitesavedcommands array
        sitesavedcommands.push(newWriteCmd);

        // Write out to the site data, redraw gui
        svCmdReadWrite("write","SVDCMDS",site["SITEID"]);
        svRedrawCmdGUI();
    }
}

// future use
function svCmdArrayUpdate(sitesavedcommands)
{
    // try to avoid wiping it all
    if (sitesavedcommands.length < 1) {
       WriteLog("Fatal error with saved commands data; aborting.");
       return;
    }
}

function svCmdTopMenu(tmptopmenu,side)
{
   var cmdmenu = document.getElementById(side+"cmd");
   var site = (side=="left")?lsite:rsite;
   if (tmptopmenu) {
      // local variable so editing doesn't break the menu
      var menusavedcommands = tmptopmenu.slice();
      // clear the menu
      while (cmdmenu.childNodes.length > 0) {
        cmdmenu.removeChild(cmdmenu.lastChild);
      }
      // repopulate with default + saved
      var staticcmds = ["refresh|refresh","filter|filter",
         "selectall|select all","clearall|clear all","site|site cmd",
         "mkdir|new dir","unlink|delete","rename|rename","compare|compare",
         "copylist|copy list"];
      for (var i = 0; i < staticcmds.length; i++){
         var option = document.createElement("option");
         option.value = staticcmds[i].split("|")[0];
         if (option.value == "site") {
             option.selected = true;
         }
         option.text = staticcmds[i].split("|")[1];
         cmdmenu.appendChild(option);
      }
      // add saved commands
      // site name first as divider
      var option = document.createElement("option");
      option.value = "separator"
      option.text = "-"+sitelist[site.siteid]["NAME"]+"-";
      option.disabled = true;
      cmdmenu.appendChild(option);
      // saved commands, use custom savecmd tag for args in doCommand
      for (var i = 0; i < menusavedcommands.length; i++) {
         var option = document.createElement("option");
         option.value = menusavedcommands[i].split(" ")[0];
         option.text = menusavedcommands[i];
         var tmpcmd = menusavedcommands[i];
         savedcmdstr = tmpcmd.substr(tmpcmd.indexOf(" ") + 1);
         option.setAttribute('savedcmd',savedcmdstr);
         option.text = menusavedcommands[i];
         cmdmenu.appendChild(option);
      }
   }
}

function svCmdReadWrite(rwcommand,type,siteid)
{
    var site = sitelist[siteid];
    var tmpsavedcmds;
    var encodeddata = '';
    var side;
    if (siteid == lsite.siteid) {
        side = "left";
    }else if (siteid == rsite.siteid) {
        side = "right";
    }

    if (rwcommand == "read") {
        encodeddata = site[type];
        if(!encodeddata || encodeddata == '' || encodeddata == 'undefined') {
            // WriteLog("Cannot read " + type + " from " + sitelist[siteid]["NAME"]); 
            return -1;
        }
        if (type == "SVDCMDS") {
            tmpsavedcmds = b64wrap(encodeddata,"decode");
            if(tmpsavedcmds) {
                tmpsitesavedcommands = tmpsavedcmds.split(",");
                return tmpsitesavedcommands;
            }else{ return -1; }
        }else if (type == "RDIRS") {
            tmpsavedcmds = b64wrap(encodeddata,"decode");
            if(tmpsavedcmds) {
                siterecentdirs = tmpsavedcmds.split(",");
            }else{ return -1; }
        }
    } else if (rwcommand == "write") {
        if (type == "SVDCMDS") {
            encodedsitecommands = b64wrap(sitesavedcommands,"encode");
            if(encodedsitecommands) {
                save = "SITEMOD|SITEID=" + siteid + "|SVDCMDS=" + encodedsitecommands;
                socket.send(save+"\n");
                // update the site{} data
                sitelist[siteid]["SVDCMDS"] = encodedsitecommands;
                // Also update top level menus if site matches
                if (side) {
                    svCmdTopMenu(sitesavedcommands,side);
                }
            }
        }
        if (type == "RDIRS") {
            tmpsavedcmds = eval(side+"siderecentdirs").toString();
            var tmpencodedrecentdirs = b64wrap(tmpsavedcmds,"encode");
            if(tmpencodedrecentdirs) {
               save = "SITEMOD|SITEID=" + siteid + "|RDIRS=" + tmpencodedrecentdirs;
            } else { save = "SITEMOD|SITEID=" + siteid; }
            // WriteLog("Saved rdir site data: " + tmpsavedcmds);
            socket.send(save+"\n");
        }
    }
}

function svUpdateCommand(updateid)
{
    sitesavedcommands[updateid] = document.getElementById("sv_args"+updateid).value;
    svCmdReadWrite("write","SVDCMDS",site["SITEID"]);
}

function svDeleteCommand(delid)
{
    sitesavedcommands.splice(delid, 1);
    svRedrawCmdGUI();
    svCmdReadWrite("write","SVDCMDS",site["SITEID"]);
}

function svMoveCommand(arrayid,direction)
{
    switch(direction) {
    case 'up':
        if (arrayid != 0) {
            var ahead = +arrayid - 1;
            var aheadval = document.getElementById("sv_args"+ahead).value;
            sitesavedcommands[ahead] = document.getElementById("sv_args"+arrayid).value;
            sitesavedcommands[arrayid] = aheadval;
            svRedrawCmdGUI();
            svCmdReadWrite("write","SVDCMDS",site["SITEID"]);
        }
        break;
    case 'down':
        if (arrayid != sitesavedcommands.length) {
            var ahead = +arrayid + 1;
            var aheadval = document.getElementById("sv_args"+ahead).value;
            sitesavedcommands[ahead] = document.getElementById("sv_args"+arrayid).value;
            sitesavedcommands[arrayid] = aheadval;
            svRedrawCmdGUI();
            svCmdReadWrite("write","SVDCMDS",site["SITEID"]);
        }
        break;
    case 'top':
        sitesavedcommands.splice(arrayid,1);
        sitesavedcommands.unshift(document.getElementById("sv_args"+arrayid).value);
        svRedrawCmdGUI();
        svCmdReadWrite("write","SVDCMDS",site["SITEID"]);
        break;
    case 'bottom':
        sitesavedcommands.splice(arrayid,1);
        sitesavedcommands.push(document.getElementById("sv_args"+arrayid).value);
        svRedrawCmdGUI();
        svCmdReadWrite("write","SVDCMDS",site["SITEID"]);
        break;
    default:
        break;
    }
}

function svRedrawCmdGUI()
{
    while (saved_commands.childNodes.length > 0) {
        saved_commands.removeChild(saved_commands.lastChild);
    }

    for (var i = 0; i < sitesavedcommands.length; i++) {
        var cmd = sitesavedcommands[i];
        if( cmd == null) {
            return;
        }
        svDrawCmdGUI(cmd,i);
    }
}

function svDrawCmdGUI(command,arraynum)
{
    // div wrapper
    var cmdwrapper = document.createElement("div");
    // textbox
    var cmdinput = document.createElement("input");
    // images
    var imgsave = document.createElement("input");
    var imgup = document.createElement("input");
    var imgdelete = document.createElement("input");
    var imgdown = document.createElement("input");
    var imgtop = document.createElement("input");
    var imgbottom = document.createElement("input");

    cmdwrapper.type = "div";
    cmdwrapper.id = "cmdwrapper" + arraynum;
    cmdwrapper.title = "Command wrapper" + arraynum;
    document.getElementById("saved_commands").appendChild(cmdwrapper);

    var cmdinput = document.createElement("input");
    cmdinput.type = "text";
    cmdinput.id = "sv_args" + arraynum;
    cmdinput.title = "Command arguments" ;
    cmdinput.value = command;
    document.getElementById("cmdwrapper" + arraynum).appendChild(cmdinput);

    // images save,delete,up,down,top,bottom
    imgsave.type = 'image';
    imgsave.className = 'ctrls';
    imgsave.src = "img/save.gif";
    imgsave.title = "Update saved command";
    imgsave.id = arraynum;
    imgsave.setAttribute('onclick', 'svUpdateCommand(this.id);');
    document.getElementById("cmdwrapper" + arraynum).appendChild(imgsave);

    imgdelete.type = "image";
    imgdelete.className = "ctrls";
    imgdelete.src = "img/delete_item.png";
    imgdelete.title = "Delete saved command";
    imgdelete.id = arraynum;
    imgdelete.setAttribute('onclick', 'svDeleteCommand(this.id);');
    document.getElementById("cmdwrapper" + arraynum).appendChild(imgdelete);

    imgup.type = "image";
    imgup.className = "ctrls";
    imgup.src = "img/move-up.png";
    imgup.title = "Move up";
    imgup.id = arraynum;
    imgup.setAttribute('onclick', 'svMoveCommand(this.id,"up");');
    document.getElementById("cmdwrapper" + arraynum).appendChild(imgup);

    imgdown.type = "image";
    imgdown.className = "ctrls";
    imgdown.src = "img/move-down.png";
    imgdown.title = "Move down";
    imgdown.id = arraynum;
    imgdown.setAttribute('onclick', 'svMoveCommand(this.id,"down");');
    document.getElementById("cmdwrapper" + arraynum).appendChild(imgdown);

    imgtop.type = "image";
    imgtop.className = "ctrls";
    imgtop.src = "img/move-top.png";
    imgtop.title = "Move to top";
    imgtop.id = arraynum;
    imgtop.setAttribute('onclick', 'svMoveCommand(this.id,"top");');
    document.getElementById("cmdwrapper" + arraynum).appendChild(imgtop);

    imgbottom.type = "image";
    imgbottom.className = "ctrls";
    imgbottom.src = "img/move-bottom.png";
    imgbottom.title = "Move to bottom";
    imgbottom.id = arraynum;
    imgbottom.setAttribute('onclick', 'svMoveCommand(this.id,"bottom");');
    document.getElementById("cmdwrapper" + arraynum).appendChild(imgbottom);
}

function showRecentDirs(side,action)
{
    if (side == "left") {
        var sd = "l";
    }else{ var sd = "r"; }

    // disable when not connected
    if (eval(sd+"site.session") == -1) {
        action = "invalid";
    }

    if (action == 'invalid') {
        var select = document.getElementById(sd+"dirselect");
        select.style.display = 'none';
        var textbox = document.getElementById(sd+"path");
        textbox.style.display = 'inline';
        textbox.value = '';
        return;
    }

    if (action == 'show') {
        if (eval(side+"siderecentdirs").length == 0) {
            return;
        }
        var textbox = document.getElementById(sd+"path");
        textbox.style.display = 'none';
        var select = document.getElementById(sd+"dirselect");

        //wipe first
        while (select.childNodes.length > 0) {
            select.removeChild(select.lastChild);
        }
        //placeholder for going back
        var option = document.createElement("option");
        option.value = "--placeholder--";
        option.text = "-Recent directories-";
        eval(sd+"dirselect").appendChild(option);

        select.style.display = 'inline';
        for (var i = 0; i < eval(side+"siderecentdirs").length; i++) {
           var option = document.createElement("option");
           option.value = eval(side+"siderecentdirs")[i];
           option.text = eval(side+"siderecentdirs")[i];
           eval(sd+"dirselect").appendChild(option);
        }
        // deselect all so placeholder works
        document.getElementById(sd+"dirselect").selectedIndex = "-1";
    }
    if (action == 'hide') {
        var select = document.getElementById(sd+"dirselect");
        select.style.display = 'none';
        var textbox = document.getElementById(sd+"path");
        textbox.style.display = 'inline';
        var selecteddir = document.getElementById(sd+"dirselect").value;
        if (selecteddir == "--placeholder--") {
            return;
        }
        document.getElementById(sd+"path").value = selecteddir; 
        doCWDEdit(side);
    }
}

function rememberCWD(side,dir)
{
    var site = (side=="left")?lsite:rsite;
    var sitename = sitelist[site.siteid]["NAME"];
    if (dir == '..'){
       return;
    }else if (dir == sitelist[site.siteid]["STARTDIR"]) {
        return;
    }
    // oft used dirs to top
    for (var i = 0; i < eval(side+"siderecentdirs").length; i++) {
        if (dir == eval(side+"siderecentdirs")[i]) {
            eval(side+"siderecentdirs").splice(i,1);
        }
    }
    if (eval(side+"siderecentdirs").length < 10) {
        eval(side+"siderecentdirs").unshift(dir);
    }else{
        eval(side+"siderecentdirs").pop();
        eval(side+"siderecentdirs").unshift(dir);
    }
}
