// everything else except control
/* eslint-env browser */
/* global wsUri */
/* global myuser */
/* global mypass */
/* global output */
/* global qmoutput */
/* global lwalloutput */
/* global rwalloutput */
/* global socket */
/* global currview */
/* global sitelist */
/* global qlist */
/* global lsite */
/* global rsite */
/* global queue */
/* global queuemanager */
/* global connected */
/* global debug */
/* global eventqueue */
/* global usequeue */
/* global sitesavedcommands */
/* global siterecentdirs */
/* global encodedsitecommands */
/* global encodedrecentdirs */
/* global leftsiderecentdirs */
/* global rightsiderecentdirs */
/* global selectedSite */
/* global queuenewlock */
/* global firstload */
/* global selectedlsiteid */
/* global selectedrsiteid */
/* global queuesidesid */
/* global expandstart */

/* global handlewrap */
/* global doQMRefresh */
/* global AnsiUp */

function Dispatch(line) {
  if(!line||line.length===0) {
    BadMsg(line); return;
  }
  const bits = line.split('|');
  if(bits.length<1) {
    BadMsg(line); return;
  }
  const cmd = bits.shift();
  let data = {};
  if(bits.length>0) {
    data=ParseData(bits);
  }
  data['func'] = cmd.toUpperCase();
  handlewrap(cmd,data);
}
function ParseData(stuff) {
  const params = {}; const len=stuff.length;
  for(let n = 0; n < len; n++) {
    const pair = stuff[n].split('=');
    params[chomp(pair[0].toUpperCase())]=(pair[1])?chomp(pair[1]):'';
    //params[pair[0].toLowerCase()]=chomp(pair[1])||"";
  }
  return params;
}
function BadMsg(msg) {
  WriteLog('Received bad/unknown response: '+msg)
}
function WriteLog(message) {
  //message += "\n";
  //output.value+=message;
  // limit output scrollback to 500 lines
  if(document.getElementById('output')) {
    const splitlog = output.innerHTML.split('<br>');
    if(splitlog.length > 500) {
      splitlog.splice(0,1);
      output.innerHTML = splitlog.join('<br />');
    }
  }
  message += '</br>';
  output.innerHTML+=message;
  output.scrollTop = output.scrollHeight;
}
function WriteQMLog(message) {
  message += '\n';
  qmoutput.value+=message;
  qmoutput.scrollTop = qmoutput.scrollHeight;
}
function WriteLargeLog(side,message) {
  //message += "\n";
  message += '</br>';
  if(side === 'left') {
    //lwalloutput.value+=message;
    lwalloutput.innerHTML+=message;
    lwalloutput.scrollTop = lwalloutput.scrollHeight;
  } else if(side === 'right') {
    //rwalloutput.value+=message;
    rwalloutput.innerHTML+=message;
    rwalloutput.scrollTop = rwalloutput.scrollHeight;
  }
}
function chomp(str) {
  return str.replace(/(\n|\r)+$/, '');
}
function toggleSblock() {
  const sblock = document.getElementById('sblock');
  if(sblock.style.display === 'block') {
    sblock.style.display = 'none';
  } else {
    sblock.style.display = 'block';
  }
}
function clearLog(area) {
  if(area === 'qmoutput') {
    document.getElementById(area).value='';
  } else {
    document.getElementById(area).innerHTML='';
  }
}
function hideLog(state) {
  const out = document.getElementById('output');
  if(!state) {
    if(out.style.display === 'block') {
      state='none';
    } else {
      state='block';
    }
  }
  if(state === 'none') {
    document.getElementById('logtog').innerHTML = '--show--';
    out.style.display = 'none';
  } else {
    document.getElementById('logtog').innerHTML = '--hide--';
    out.style.display = 'block';
  }
  localStorage.logstate = out.style.display;
}

function clearTable(id) {
  const obj = document.getElementById(id);
  const toDeleteHeader = true;
  const limit = toDeleteHeader ? 0 : 1;
  if(!obj && !obj.rows) {
    return;
  }
  const rows = obj.rows;
  if(limit > rows.length) {
    return;
  }
  for(; rows.length > limit;) {
    obj.deleteRow(limit);
  }
}
function addTableRow(tableID,flag) {
  const table = document.getElementById(tableID);
  const rowCount = table.tBodies[0].rows.length;
  const row = table.tBodies[0].insertRow(rowCount);
  let prefix;
  row.setAttribute('class',flag);
  for(let i=2; i<arguments.length; i++) {
    const cell = row.insertCell(i-2);
    const dv = document.createElement('div');
    dv.setAttribute('class','nw');
    dv.innerHTML = arguments[i][0];
    dv.setAttribute('title',arguments[i][1]);
    cell.appendChild(dv);
    switch(tableID) {
    case 'queue':
      prefix = 'q';
      break;
    case 'qmqueue':
      prefix = 'qm';
      break;
    default:
      prefix = '';
      break;
    }
    cell.setAttribute('class',prefix+'td'+(i-1).toString());
  }
}
function dirlistStatus(tableID,message) {
  // assume table is empty!!!
  const table = document.getElementById(tableID);
  const row = table.tBodies[0].insertRow(0);
  row.setAttribute('colspan','3');
  const cell = row.insertCell(0);
  const dv = document.createElement('div');
  dv.setAttribute('class','nwcenter');
  row.setAttribute('class','processing');
  dv.innerHTML = message;
  cell.appendChild(dv);
}

// Move me somewhere better
function time2str(UNIXTimestamp) {
  // FTP/Unix 'ls' dates are somewhat annoying. If the date are +- 6 months
  // from now, it uses "Sep  5 14:51". Otherwise it picks the less accurate
  // "Feb 27  2006"
  const a = new Date(UNIXTimestamp*1000);
  const months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  const milliseconds = new Date().getTime();
  const halfyear = 183 * 24 * 60 * 60 * 1000;
  let d = ''+a.getDate();
  d = Array(2 + 1 - d.length).join('0') + d;

  if((a.getTime() > (milliseconds + halfyear)) ||
        (a.getTime() < (milliseconds - halfyear))) {
    return months[a.getMonth()] + ' ' + d + '  ' + a.getFullYear();
  }

  let hours = ''+a.getHours();
  let mins = ''+a.getMinutes();
  hours = Array(2 + 1 - hours.length).join('0') + hours;
  mins = Array(2 + 1 - mins.length).join('0') + mins;

  return months[a.getMonth()] + ' ' + d + ' ' + hours + ':' + mins;
}

// Move me somewhere better
function decode(str) {
  return decodeURIComponent((str+'').replace(/\+/g, '%20'));
  //return unescape(str.replace(/\+/g, " "));
}
function encode(str) {
  return encodeURIComponent((str+''));
}
function changeCSS(selector,property,value) {
  const rules = document.styleSheets[0].cssRules;
  if(rules) {
    for(let i=0; i<rules.length; i++) {
      if(rules[i].selectorText === selector) {
        rules[i].style.setProperty(property,value,'');
      }
    }
  }
}

function cleanUI() {
  const newwidth = (window.innerWidth/2)-20;
  const threequarter = (window.innerWidth*0.75);
  changeCSS('.output','width',threequarter+'px');
  changeCSS('.header','width',newwidth+'px');
  changeCSS('.tbody','width',newwidth+'px');
  changeCSS('.scroll_table','width',newwidth+'px');
  changeCSS('.head1','width',Math.round(newwidth*0.65)+'px');
  changeCSS('.head2','width',Math.round(newwidth*0.10)+'px');
  changeCSS('.td1','width',Math.round(newwidth*0.65)+'px');
  changeCSS('.td2','width',Math.round(newwidth*0.10)+'px');
  changeCSS('.qheader','width',threequarter+'px');
  changeCSS('.qwrapper','width',threequarter+'px');
  changeCSS('.qscroll_table','width',threequarter+'px');
  changeCSS('.qtbody','width',threequarter+'px');
  changeCSS('.qhead1','width',Math.round(threequarter*0.35)+'px');
  changeCSS('.qhead2','width',Math.round(threequarter*0.10)+'px');
  changeCSS('.qhead3','width',Math.round(threequarter*0.10)+'px');
  changeCSS('.qtd1','width',Math.round(threequarter*0.35)+'px');
  changeCSS('.qtd2','width',Math.round(threequarter*0.10)+'px');
  changeCSS('.qtd3','width',Math.round(threequarter*0.10)+'px');
  hideLog(localStorage.logstate);
  // queue manager below
  changeCSS('.qmoutput','width',threequarter+'px');
  changeCSS('.qmheader','width',threequarter+'px');
  changeCSS('.qmwrapper','width',threequarter+'px');
  changeCSS('.qmscroll_table','width',threequarter+'px');
  changeCSS('.qmtbody','width',threequarter+'px');
  changeCSS('.qmhead1','width',Math.round(threequarter*0.02)+'px');
  changeCSS('.qmhead2','width',Math.round(threequarter*0.20)+'px');
  changeCSS('.qmhead3','width',Math.round(threequarter*0.20)+'px');
  changeCSS('.qmhead4','width',Math.round(threequarter*0.10)+'px');
  changeCSS('.qmhead5','width',Math.round(threequarter*0.10)+'px');
  changeCSS('.qmtd1','width',Math.round(threequarter*0.02)+'px');
  changeCSS('.qmtd2','width',Math.round(threequarter*0.20)+'px');
  changeCSS('.qmtd3','width',Math.round(threequarter*0.20)+'px');
  changeCSS('.qmtd4','width',Math.round(threequarter*0.10)+'px');
  changeCSS('.qmtd5','width',Math.round(threequarter*0.10)+'px');
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

function changeSort(side,stype) {
  const csort = localStorage[side+'sort'].split('-');
  let nsort = '';
  if(csort[0] === stype) {  // toggle asc/dsc
    nsort = stype+((csort[1] === 'ASC')?'-DSC':'-ASC');
  } else {  // default descending for date, ascending for name
    if(stype === 'NAME') {
      nsort = stype+'-ASC';
    } else {
      nsort = stype+'-DSC';
    }
  }
  localStorage[side+'sort'] = nsort;
  refreshTable(side);
}
function refreshTable(side) {
  clearTable(side);
  if(side === 'queue') {
    for(let i = 0; i < queue.listing.length; i++) {
      const mydat = queue.listing[i];
      const src = decode(mydat['SRCPATH']);
      const dst = decode(mydat['DSTPATH']);
      if(mydat['FID']) {
        const fid = decode(mydat['FID']);
      }
      let size;
      if(mydat['SIZE']) {
        size = decode(mydat['SIZE']);
      } else {
        if(mydat['SRCSIZE']) {
          size = decode(mydat['SRCSIZE']);
        }
      }
      const date = decode(mydat['DATE']);
      addTableRow('queue','',
        ["<input type='checkbox' name='QITEM#"+mydat['@']+"' value='QITEM#"+mydat['@']+"'/>"+src,src],
        [size,''], // size
        [date,''], // date
        [dst,dst]);
    }
  } else if(side === 'qmqueue') {
    for(let i = 0; i < queuemanager.listing.length; i++) {
      const mydat = queuemanager.listing[i];
      const src = mydat['NORTH'];
      const dst = mydat['SOUTH'];
      const items = mydat['ITEMS']+'/'+mydat['ERRORS'];
      addTableRow('qmqueue',('SUBSCRIBED' in mydat) ? 'dir' : 'file',
        ["<input type='checkbox' name='QID#"+mydat['QID']+"' value='QID#"+mydat['QID']+"'/>", ('SUBSCRIBED' in mydat) ? 'Subscribed' : 'Not subscribed'],
        [src,src],
        [dst,dst],
        [items,items],
        [mydat['KB/S'], mydat['KB/s']],
        [mydat['STATUS'], mydat['STATUS']]);
    }
  } else {
    const sitedata = (side==='left')?lsite:rsite;
    addTableRow(side,'dir',
      ["<input class='chkbox' type='checkbox' name='FID#-1' value='FID#-1' disabled='true'/><a href=\"#\" onclick=\"doCWD('"+sitedata.session+"','..');\">..</a>",'Go up a directory'],
      ['',''],
      ['','']);
    const dirlist = [];
    if(!localStorage[side+'sort']) {
      localStorage[side+'sort'] = 'DATE-DSC';
    }
    const sorttype = localStorage[side+'sort'].split('-');
    for(let i = 0; i < sitedata.listing.length; i++) {
      dirlist.push([i, sitedata.listing[i][sorttype[0]]]);
    }
    dirlist.sort(function(a, b) {
      a = a[1]; b = b[1]; return a < b ? -1 : (a > b ? 1 : 0);
    });
    if(sorttype[1] === 'DSC') {
      dirlist.reverse();
    }
    for(let i = 0; i < dirlist.length; i++) {
      const key = dirlist[i][0];
      const value = dirlist[i][1];
      const mydat = sitedata.listing[key];
      const datestr = time2str(mydat['DATE']);
      const name = decode(mydat['NAME']);
      const alt = mydat['PERM'] + ' ' + mydat['USER'] + ' ' + mydat['GROUP'] + ' ' + mydat['SIZE'] + ' ' + datestr + ' ' + name;
      // Actually, filename is URL encoded and should be decoded
      if(mydat['FTYPE'] === 'directory') {
        addTableRow(side,'dir',
          ["<input class='chkbox' type='checkbox' name='FID#"+mydat['FID']+"' value='FID#"+mydat['FID']+"'/><a href=\"#\" onclick=\"doCWD('"+sitedata.session+"','"+mydat['NAME']+"');\">"+name+'</a>',name],
          [mydat['SIZE'],mydat['SIZE']],
          [datestr,alt]);
      } else {
        addTableRow(side,'file',["<input class='chkbox' type='checkbox' name='FID#"+mydat['FID']+"' value='FID#"+mydat['FID']+"'/>"+name,name],[mydat['SIZE'],mydat['SIZE']],[datestr,alt]);
      }
    }
  }
  cleanUI();
}
function move(array, oldIndex, newIndex) {
  while(oldIndex < 0) {
    oldIndex += array.length;
  }
  while(newIndex < 0) {
    newIndex += array.length;
  }
  if(newIndex >= array.length) {
    let k = newIndex - array.length;
    while((k--) + 1) {
      array.push(undefined);
    }
  }
  array.splice(newIndex, 0, array.splice(oldIndex, 1)[0]);
  return array; // for testing purposes
};

function toggleView(view) {
  //if(!connected&&view != "none"){
  //   document.getElementById("connectstatus").innerHTML = "Must connect before doing anything.";
  //   return;
  // }
  if(view === 'primary') {
    changeCSS('#primary','display','block'); document.getElementById('pmtab').setAttribute('class','selected');
    changeCSS('#sitemanager','display','none'); document.getElementById('smtab').setAttribute('class','');
    changeCSS('#queuemanager','display','none'); document.getElementById('qmtab').setAttribute('class','');
    changeCSS('#logviewer','display','none'); document.getElementById('lvtab').setAttribute('class','');
  } else if(view === 'sitemanager') {
    changeCSS('#primary','display','none'); document.getElementById('pmtab').setAttribute('class','');
    changeCSS('#sitemanager','display','block'); document.getElementById('smtab').setAttribute('class','selected');
    changeCSS('#queuemanager','display','none'); document.getElementById('qmtab').setAttribute('class','');
    changeCSS('#logviewer','display','none'); document.getElementById('lvtab').setAttribute('class','');
  } else if(view === 'queuemanager') {
    changeCSS('#primary','display','none'); document.getElementById('pmtab').setAttribute('class','');
    changeCSS('#sitemanager','display','none'); document.getElementById('smtab').setAttribute('class','');
    changeCSS('#queuemanager','display','block'); document.getElementById('qmtab').setAttribute('class','selected');
    changeCSS('#logviewer','display','none'); document.getElementById('lvtab').setAttribute('class','');
    doQMRefresh();
  } else if(view === 'logviewer') {
    changeCSS('#primary','display','none'); document.getElementById('pmtab').setAttribute('class','');
    changeCSS('#sitemanager','display','none'); document.getElementById('smtab').setAttribute('class','');
    changeCSS('#queuemanager','display','none'); document.getElementById('qmtab').setAttribute('class','');
    changeCSS('#logviewer','display','block'); document.getElementById('lvtab').setAttribute('class','selected');
  } else {
    changeCSS('#primary','display','none'); document.getElementById('pmtab').setAttribute('class','');
    changeCSS('#sitemanager','display','none'); document.getElementById('smtab').setAttribute('class','');
    changeCSS('#queuemanager','display','none'); document.getElementById('qmtab').setAttribute('class','');
    changeCSS('#logviewer','display','none'); document.getElementById('lvtab').setAttribute('class','');
  }
}

function togglePwdChange() {
  const pwdblock = document.getElementById('pwdchbox');
  if(pwdblock.style.display === 'block') {
    pwdblock.style.display = 'none';
  } else {
    pwdblock.style.display = 'block';
  }
}

function doPwdChange() {
  const opass = document.getElementById('currpass').value;
  const npass = document.getElementById('newpass').value;

  changeCSS('.pwdchbox','display','none');
  if(document.getElementById('confpass').value !== npass) {
    alert('The new and confirm passwords do not match\n');
    return;
  }

  socket.send('SETPASS|OLD='+opass+'|NEW='+npass+'\n');

  togglePwdChange();
}

function b64wrap(str,command) {
  if(command === 'encode') {
    const tmpstr = b64EncodeUnicode(str);
    if(tmpstr) {
      return b64prep(tmpstr,'encode');
    }
  }
  if(command === 'decode') {
    const tmpstr = b64prep(str,'decode');
    if(tmpstr) {
      return b64DecodeUnicode(tmpstr,'decode');
    }
  }
}

function b64prep(str,command) {
  if(command === 'encode') {
    return str.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
  } else if(command === 'decode') {
    if(str.length % 4 !== 0) {
      str += ('===').slice(0, 4 - (str.length % 4));
      return str.replace(/-/g, '+').replace(/_/g, '/');
    } else {
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
  const ansiUp = new AnsiUp();
  const ansitext = ansiUp.ansi_to_html(text);
  return ansitext;
}

///////////////////STUFF BELOW HERE IS NO LONGER USED//////////////////////////
