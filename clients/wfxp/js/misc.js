// everything else except control

function Dispatch(line){
    if(!line||line.length==0) {BadMsg(line);return;}
    var bits = line.split("|");
    if(bits.length<1) {BadMsg(line);return;}
    var cmd = bits.shift();
    var data = {};
    if(bits.length>0) {data=ParseData(bits);}
    data["func"] = cmd.toUpperCase();
    handlewrap(cmd,data);
}
function ParseData(stuff){
  var params = {}; var len=stuff.length;
  for(var n = 0; n < len; n++){
    var pair = stuff[n].split("=");
      params[pair[0].toUpperCase().chomp()]=(pair[1])?pair[1].chomp():"";
    //params[pair[0].toLowerCase()]=pair[1].chomp()||"";
  }
  return params;
}
function BadMsg(msg){
  WriteLog("Received bad/unknown response: "+msg)
}
function WriteLog(message) {
  //message += "\n";
  //output.value+=message;
  message += "</br>";
  output.innerHTML+=message;
  output.scrollTop = output.scrollHeight;
}
function WriteQMLog(message) {
  message += "\n";
  qmoutput.value+=message;
  qmoutput.scrollTop = qmoutput.scrollHeight;
}
function WriteLargeLog(side,message) {
  //message += "\n";
  message += "</br>";
  if (side == "left") {
      //lwalloutput.value+=message;
      lwalloutput.innerHTML+=message;
      lwalloutput.scrollTop = lwalloutput.scrollHeight;
  } else if (side == "right") {
      //rwalloutput.value+=message;
      rwalloutput.innerHTML+=message;
      rwalloutput.scrollTop = rwalloutput.scrollHeight;
  }
}
String.prototype.chomp = function () {
return this.replace(/(\n|\r)+$/, '');
}
function toggleSblock(){
   var sblock = document.getElementById("sblock");
   if(sblock.style.display == "block"){
      sblock.style.display = "none";
   }else{
      sblock.style.display = "block";
   }
}
function clearLog(area){
   if(area == "qmoutput") {
      document.getElementById(area).value="";
   } else {
      document.getElementById(area).innerHTML="";
   }
}
function hideLog(state){
   var out = document.getElementById("output");
   if(!state){
      if(out.style.display == "block"){
         state="none";
      }else{
         state="block";
      }
   }
   if(state == "none"){
      document.getElementById("logtog").innerHTML = "--show--";
      out.style.display = "none";
   }else{
      document.getElementById("logtog").innerHTML = "--hide--";
      out.style.display = "block";
   }
   localStorage.logstate = out.style.display;
}

function clearTable(id) {
   var obj = document.getElementById(id);
   var toDeleteHeader = true;
   var limit = !!toDeleteHeader ? 0 : 1;
   if(!obj && !obj.rows)
      return;
   var rows = obj.rows;
   if(limit > rows.length)
      return;
   for(; rows.length > limit; ) {
      obj.deleteRow(limit);
   }
}
function addTableRow(tableID,flag) {
   var table = document.getElementById(tableID);
   var rowCount = table.tBodies[0].rows.length;
   var row = table.tBodies[0].insertRow(rowCount);
   var prefix;
   row.setAttribute("class",flag);
   for (var i=2; i<arguments.length; i++) {
      var cell = row.insertCell(i-2);
      var dv = document.createElement("div");
      dv.setAttribute("class","nw");
      dv.innerHTML = arguments[i][0];
      dv.setAttribute("title",arguments[i][1]);
      cell.appendChild(dv);
       switch(tableID) {
       case "queue":
           prefix = "q";
           break;
       case "qmqueue":
           prefix = "qm";
           break;
       default:
           prefix = "";
           break;
       }
      cell.setAttribute("class",prefix+"td"+(i-1).toString());
   }
}
function dirlistStatus(tableID,message){
   // assume table is empty!!!
   var table = document.getElementById(tableID);
   var row = table.tBodies[0].insertRow(0);
   row.setAttribute("colspan","3");
   var cell = row.insertCell(0);
   var dv = document.createElement("div");
   dv.setAttribute("class","nwcenter");
   row.setAttribute("class","processing");
   dv.innerHTML = message;
   cell.appendChild(dv);
}

// Move me somewhere better
function time2str(UNIX_timestamp){
    // FTP/Unix 'ls' dates are somewhat annoying. If the date are +- 6 months
    // from now, it uses "Sep  5 14:51". Otherwise it picks the less accurate
    // "Feb 27  2006"
    var a = new Date(UNIX_timestamp*1000);
    var months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
    var milliseconds = new Date().getTime();
    var halfyear = 183 * 24 * 60 * 60 * 1000;
    var d = ""+a.getDate();
    d = Array(2 + 1 - d.length).
        join("0")
        + d;

    if ((a.getTime() > (milliseconds + halfyear)) ||
        (a.getTime() < (milliseconds - halfyear))) {
        return months[a.getMonth()] + " " + d + "  " + a.getFullYear();
    }

    var hours = ""+a.getHours();
    var mins = ""+a.getMinutes();
    hours = Array(2 + 1 - hours.length).join("0") + hours;
    mins  = Array(2 + 1 - mins.length).join("0") + mins;

    return months[a.getMonth()] + " " + d + " " + hours + ":" + mins;
}

// Move me somewhere better
function decode(str) {
    return decodeURIComponent((str+'').replace(/\+/g, '%20'));
    //return unescape(str.replace(/\+/g, " "));
}
function encode(str) {
    return encodeURIComponent((str+''));
}
function changeCSS(selector,property,value){
    var rules = document.styleSheets[0].cssRules;
    if (rules) {
        for (i=0; i<rules.length; i++) {
            if (rules[i].selectorText == selector) {
                rules[i].style.setProperty(property,value,'');
            }
        }
    }
}

function cleanUI(){
  var newwidth = (window.innerWidth/2)-20;
  var threequarter = (window.innerWidth*0.75);
  changeCSS(".output","width",threequarter+"px");
  changeCSS(".header","width",newwidth+"px");
  changeCSS(".tbody","width",newwidth+"px");
  changeCSS(".scroll_table","width",newwidth+"px");
  changeCSS(".head1","width",Math.round(newwidth*0.65)+"px");
  changeCSS(".head2","width",Math.round(newwidth*0.10)+"px");
  changeCSS(".td1","width",Math.round(newwidth*0.65)+"px");
  changeCSS(".td2","width",Math.round(newwidth*0.10)+"px");
  changeCSS(".qheader","width",threequarter+"px");
  changeCSS(".qwrapper","width",threequarter+"px");
  changeCSS(".qscroll_table","width",threequarter+"px");
  changeCSS(".qtbody","width",threequarter+"px");
  changeCSS(".qhead1","width",Math.round(threequarter*0.35)+"px");
  changeCSS(".qhead2","width",Math.round(threequarter*0.10)+"px");
  changeCSS(".qhead3","width",Math.round(threequarter*0.10)+"px");
  changeCSS(".qtd1","width",Math.round(threequarter*0.35)+"px");
  changeCSS(".qtd2","width",Math.round(threequarter*0.10)+"px");
  changeCSS(".qtd3","width",Math.round(threequarter*0.10)+"px");
  hideLog(localStorage.logstate);
  // queue manager below
  changeCSS(".qmoutput","width",threequarter+"px");
  changeCSS(".qmheader","width",threequarter+"px");
  changeCSS(".qmwrapper","width",threequarter+"px");
  changeCSS(".qmscroll_table","width",threequarter+"px");
  changeCSS(".qmtbody","width",threequarter+"px");
  changeCSS(".qmhead1","width",Math.round(threequarter*0.02)+"px");
  changeCSS(".qmhead2","width",Math.round(threequarter*0.20)+"px");
  changeCSS(".qmhead3","width",Math.round(threequarter*0.20)+"px");
  changeCSS(".qmhead4","width",Math.round(threequarter*0.10)+"px");
  changeCSS(".qmhead5","width",Math.round(threequarter*0.10)+"px");
  changeCSS(".qmtd1","width",Math.round(threequarter*0.02)+"px");
  changeCSS(".qmtd2","width",Math.round(threequarter*0.20)+"px");
  changeCSS(".qmtd3","width",Math.round(threequarter*0.20)+"px");
  changeCSS(".qmtd4","width",Math.round(threequarter*0.10)+"px");
  changeCSS(".qmtd5","width",Math.round(threequarter*0.10)+"px");

}

function numsort(a, b) {
          a = parseInt(a[1]);
          b = parseInt(b[1]);
          return a < b ? -1 : (a > b ? 1 : 0);
}
function strsort(a, b) {
          a = a[1];
          b = b[1];

          return a < b ? -1 : (a > b ? 1 : 0);
}

function changeSort(side,stype){
   var csort = localStorage[side+"sort"].split("-");
   var nsort = "";
   if(csort[0] == stype){  // toggle asc/dsc
     nsort = stype+((csort[1] == "ASC")?"-DSC":"-ASC");
   }else{  // default descending for date, ascending for name
     if (stype === "NAME"){
         nsort = stype+"-ASC";
     }else{
         nsort = stype+"-DSC";
     }
   }
   localStorage[side+"sort"] = nsort;
   refreshTable(side);
}
function refreshTable(side){
    clearTable(side);
    if(side == "queue"){
        var sitedata = (side=="left")?lsite:rsite;
        var byfids = {};
        for (var i = 0; i < sitedata.listing.length; i++) {
             byfids[sitedata.listing[i]["FID"]] = sitedata.listing[i];
        }
        for (var i = 0; i < queue.listing.length; i++) {
            var mydat = queue.listing[i];
            var src = decode(mydat["SRCPATH"]);
            var dst = decode(mydat["DSTPATH"]);
            var fid = decode(mydat["FID"]);
            // initial enqueue
            if (fid !== 'undefined') {
                var size = byfids[fid]["SIZE"];
                var date = time2str(byfids[fid]["DATE"]);
            } else {
            // Queue go has started so we cannot use the FID data any longer
            // Get the rest of the data from the QC|INSERT lines
                var size = decode(mydat["SRCSIZE"]);
                var date = time2str(decode(mydat["DATE"]));
            }
            addTableRow('queue',"",
                        ["<input type='checkbox' name='QITEM#"+mydat["@"]+"' value='QITEM#"+mydat["@"]+"'/>"+src,src],
                        [size,""], // size
                        [date,""], // date
                        [dst,dst]);
        }
    } else if(side == "qmqueue"){
        for (var i = 0; i < queuemanager.listing.length; i++) {
            var mydat = queuemanager.listing[i];
            var src = mydat["NORTH"];
            var dst = mydat["SOUTH"];
            var items = mydat["ITEMS"]+"/"+mydat["ERRORS"];
            addTableRow('qmqueue',("SUBSCRIBED" in mydat) ? "dir" : "file",
                        ["<input type='checkbox' name='QID#"+mydat["QID"]+"' value='QID#"+mydat["QID"]+"'/>", ("SUBSCRIBED" in mydat) ? "Subscribed" : "Not subscribed"],
                        [src,src],
                        [dst,dst],
                        [items,items],
                        [mydat["KB/S"], mydat["KB/s"]],
                        [mydat["STATUS"], mydat["STATUS"]]);
        }
    }else{
        var sitedata = (side=="left")?lsite:rsite;
        addTableRow(side,"dir",
                       ["<input class='chkbox' type='checkbox' name='FID#-1' value='FID#-1' disabled='true'/><a href=\"#\" onclick=\"do_CWD('"+sitedata.session+"','..');\">..</a>","Go up a directory"],
                       ["",""],
                       ["",""]);
          var dirlist = [];
          if(!localStorage[side+"sort"]){localStorage[side+"sort"] = "DATE-DSC";}
          var sorttype = localStorage[side+"sort"].split("-");
          for (var i = 0; i < sitedata.listing.length; i++) { dirlist.push([i, sitedata.listing[i][sorttype[0]]]);}
          dirlist.sort(function(a, b) {a = a[1];b = b[1]; return a < b ? -1 : (a > b ? 1 : 0);});
          if(sorttype[1] == "DSC"){dirlist.reverse();}
          for (var i = 0; i < dirlist.length; i++) {
              var key = dirlist[i][0];
              var value = dirlist[i][1];
              var mydat = sitedata.listing[key];
           var datestr = time2str(mydat["DATE"]);
           var name = decode(mydat["NAME"]);
           var alt = mydat["PERM"] + " " + mydat["USER"] + " " + mydat["GROUP"] + " " + mydat["SIZE"] + " " + datestr + " " + name;
           // Actually, filename is URL encoded and should be decoded
           if (mydat["FTYPE"] == "directory") {
               addTableRow(side,"dir",
                           ["<input class='chkbox' type='checkbox' name='FID#"+mydat["FID"]+"' value='FID#"+mydat["FID"]+"'/><a href=\"#\" onclick=\"do_CWD('"+sitedata.session+"','"+mydat["NAME"]+"');\">"+name+"</a>",name],
                           [mydat["SIZE"],mydat["SIZE"]],
                           [datestr,alt]);
           } else {
               addTableRow(side,"file",["<input class='chkbox' type='checkbox' name='FID#"+mydat["FID"]+"' value='FID#"+mydat["FID"]+"'/>"+name,name],[mydat["SIZE"],mydat["SIZE"]],[datestr,alt]);
           }
          }
    }
   cleanUI();
}
Array.prototype.move = function (old_index, new_index) {
    while (old_index < 0) {
        old_index += this.length;
    }
    while (new_index < 0) {
        new_index += this.length;
    }
    if (new_index >= this.length) {
        var k = new_index - this.length;
        while ((k--) + 1) {
            this.push(undefined);
        }
    }
    this.splice(new_index, 0, this.splice(old_index, 1)[0]);
    return this; // for testing purposes
};

function toggleView(view){
   //if(!connected&&view != "none"){
   //   document.getElementById("connectstatus").innerHTML = "Must connect before doing anything.";
   //   return;
  // }
   if(view == "primary"){
      changeCSS("#primary","display","block");document.getElementById("pmtab").setAttribute("class","selected");
      changeCSS("#sitemanager","display","none");document.getElementById("smtab").setAttribute("class","");
      changeCSS("#queuemanager","display","none");document.getElementById("qmtab").setAttribute("class","");
      changeCSS("#logviewer","display","none");document.getElementById("lvtab").setAttribute("class","");
   }else if(view == "sitemanager"){
      changeCSS("#primary","display","none");document.getElementById("pmtab").setAttribute("class","");
      changeCSS("#sitemanager","display","block");document.getElementById("smtab").setAttribute("class","selected");
      changeCSS("#queuemanager","display","none");document.getElementById("qmtab").setAttribute("class","");
      changeCSS("#logviewer","display","none");document.getElementById("lvtab").setAttribute("class","");
   }else if(view == "queuemanager"){
      changeCSS("#primary","display","none");document.getElementById("pmtab").setAttribute("class","");
      changeCSS("#sitemanager","display","none");document.getElementById("smtab").setAttribute("class","");
      changeCSS("#queuemanager","display","block");document.getElementById("qmtab").setAttribute("class","selected");
      changeCSS("#logviewer","display","none");document.getElementById("lvtab").setAttribute("class","");
      do_QMRefresh();
   }else if(view == "logviewer"){
      changeCSS("#primary","display","none");document.getElementById("pmtab").setAttribute("class","");
      changeCSS("#sitemanager","display","none");document.getElementById("smtab").setAttribute("class","");
      changeCSS("#queuemanager","display","none");document.getElementById("qmtab").setAttribute("class","");
      changeCSS("#logviewer","display","block");document.getElementById("lvtab").setAttribute("class","selected");
   }else{
      changeCSS("#primary","display","none");document.getElementById("pmtab").setAttribute("class","");
      changeCSS("#sitemanager","display","none");document.getElementById("smtab").setAttribute("class","");
      changeCSS("#queuemanager","display","none");document.getElementById("qmtab").setAttribute("class","");
      changeCSS("#logviewer","display","none");document.getElementById("lvtab").setAttribute("class","");
   }
}

function togglePwdChange(){
   var pwdblock = document.getElementById("pwdchbox");
   if(pwdblock.style.display == "block"){
      pwdblock.style.display = "none";
   }else{
      pwdblock.style.display = "block";
   }
}

function doPwdChange()
{
    var opass = document.getElementById("currpass").value;
    var npass = document.getElementById("newpass").value;

    changeCSS(".pwdchbox","display","none");
    if (document.getElementById("confpass").value != npass) {
        alert("The new and confirm passwords do not match\n");
        return;
    }

    socket.send("SETPASS|OLD="+opass+"|NEW="+npass+"\n");

    togglePwdChange();
}

function b64wrap(str,command)
{
    if (command == "encode") {
        var tmpstr = b64EncodeUnicode(str);
        if (tmpstr) {
            var encodedstr = b64prep(tmpstr,"encode");
        }
        if (encodedstr) {
            tmpstr = encodedstr;
            return tmpstr;
        }
    }
    if (command == "decode") {
        var tmpstr = b64prep(str,"decode");
        if (tmpstr) {
            var decodedstr = b64DecodeUnicode(tmpstr,"decode");
        }
        if (decodedstr) {
            tmpstr = decodedstr;
            return tmpstr;
        }
    }
}

function b64prep(str,command)
{
    if(command == "encode") {
        return str.replace(/\+/g, '-').replace(/\//g, '_').replace(/\=+$/, '');
    }else if(command == "decode") {
        if (str.length % 4 != 0) {
            str += ('===').slice(0, 4 - (str.length % 4));
            return str.replace(/-/g, '+').replace(/_/g, '/');
        }else{
            return str;
        }
    }
}

function b64EncodeUnicode(str) {
    return btoa(encodeURIComponent(str).replace(/%([0-9A-F]{2})/g, function(match, p1) {
        return String.fromCharCode(parseInt(p1, 16))
    }))
}

function b64DecodeUnicode(str) {
    return decodeURIComponent(Array.prototype.map.call(atob(str), function(c) {
        return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2)
    }).join(''))
}

function TextToAnsi(text) {
    var ansi_up = new AnsiUp;
    var ansitext = ansi_up.ansi_to_html(text);
    return ansitext;
}

///////////////////STUFF BELOW HERE IS NO LONGER USED//////////////////////////
