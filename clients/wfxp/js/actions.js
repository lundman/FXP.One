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
    WriteLog("Authing...");
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
            break;
        default:
            str += "|"+key+"="+site[key];
        }
    }
    return str;
}


function doSMSiteChange()
{
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
         socket.send("SESSIONNEW|SITEID="+site.siteid+"\n");
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
        if(site.siteid == -1){ // should this not be "session" (SID) ?
            WriteLog("Site "+sitelist[site.siteid]["NAME"]+" not connected!");
        }else{
            if (queue.qid != -1){
                do_QUEUEFREE(queue.qid);
            }
            socket.send("SESSIONFREE|SID="+site.session+"\n");
        }
    }else{
        WriteLog("Not connected!");
    }
}
function do_QUEUENEW(){
   if(connected){
      socket.send("QUEUENEW|NORTH_SID="+lsite.session+"|SOUTH_SID="+rsite.session+"\n");
   }
}
function do_QUEUEFREE(qid){
    if(connected){
        socket.send("QUEUEFREE|QID="+qid+"\n");
    }
}
function do_CWD(sid,dir){
    if(connected){
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
