// all functions that do something, but not necessarily a daemon command
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
/* global site */

/* global clearTable */
/* global toggleView */
/* global WriteLog */
/* global encode */
/* global decode */
/* global time2str */
/* global saved_commands */
/* global b64wrap */
/* global dirlistStatus */
/* global ldirselect */
/* global rdirselect */
/* global sv_newcmd */

/* global onOpen */
/* global onClose */
/* global onMessage */
/* global onError */

function doConnect() {
  wsUri = document.getElementById('host').value;
  socket = new WebSocket(wsUri);
  socket.onopen = function(evt) {
    onOpen(evt)
  };
  socket.onclose = function(evt) {
    onClose(evt)
  };
  socket.onmessage = function(evt) {
    onMessage(evt)
  };
  socket.onerror = function(evt) {
    onError(evt)
  };

  document.getElementById('connectstatus').innerHTML = 'Connecting...';
}
function doClose() {
  if(socket) {
    socket.close();
  }
  queue.qid = '-1';
  lsite.session = '-1';
  rsite.session = '-1';
  clearTable('left');
  clearTable('right');
  clearTable('queue');
  toggleView('none');
}
function doAuth() {
  myuser = document.getElementById('user').value;
  mypass = document.getElementById('pass').value;
  if(mypass === 'admin') {
    alert('\nYou are still using the default password.\nPlease change it as soon as possible.');
  }
  socket.send('AUTH|USER='+myuser+'|PASS='+mypass+'\n')
  document.getElementById('connectstatus').innerHTML = 'Authenticating...';
  WriteLog('Authenticating...');
}
function doSend() {
  const mbox = document.getElementById('send');
  WriteLog('SENT: ' + mbox.value);
  socket.send(mbox.value+'\n');
  mbox.value='';
}
function doEnterConn() {
  if(document.activeElement.id === 'pass') {
    doDisConn();
  }
}
function doDisConn() {
  const butt = document.getElementById('connect');
  if(butt.value === 'Connect'||butt.value === 'Retry') {
    butt.value='Disconnect'; butt.alt='Disconnect'; butt.title='Disconnect';
    butt.src='img/connected.png';
    doConnect();
  } else {
    butt.value='Connect'; butt.alt='Connect'; butt.title='Connect';
    butt.src='img/disconnected.png';
    doClose();
  }
}
function doQueueWipe() {
  // nothing here yet
}

function doCWDEdit(side) {
  if(connected) {
    const site = (side==='left')?lsite:rsite;
    if(site.session <= 0) {
      WriteLog('Site '+sitelist[site.siteid]['NAME']+' not connected!');
    } else {
      const pd = document.getElementById(side.substr(0,1)+'path');
      WriteLog('CWD Change ' + side + ': ' + pd.value);
      const enc = encode(pd.value);
      socket.send('CWD|SID='+site.session+'|PATH='+enc+'\n');
    }
  } else {
    WriteLog('Not connected!');
  }
}

function doQueue(side) {
  if(connected) {
    const site = (side==='left')?lsite:rsite;
    // We need to have a QID to be able to queue.
    if((queue.qid === '-1') || (site.session === '-1')) {
      WriteLog('No active queue (Are both sides connected?)');
    } else {
      const pd = document.getElementById(side);
      const checks = pd.getElementsByTagName('input');
      let i;
      for(i = 0; i < checks.length; i++) {
        if(checks[i].type === 'checkbox' && checks[i].checked === true) {
          const source = (side==='left')?'NORTH':'SOUTH';
          const fid = checks[i].name.split('#',2)[1];
          socket.send('QADD|QID='+queue.qid+'|SRC='+source+'|FID='+fid+'\n');
        }
      }
    }
  } else {
    WriteLog('Not connected!');
  }
}

function setAllCheckboxes(side) {
  const pd = document.getElementById(side);
  const checks = pd.getElementsByTagName('input');
  for(let i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === false) {
      if(checks[i].name.split('#',2)[1] !== '-1') {
        checks[i].checked = true;
      }
    }
  }
}

function setCheckboxes(side, fids) {
  const pd = document.getElementById(side);
  const checks = pd.getElementsByTagName('input');
  for(let i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === false) {
      const fid = checks[i].name.split('#',2)[1];
      if(fids.indexOf(fid) >= 0) {
        checks[i].checked = true;
      }
    }
  }
}

function clearAllCheckboxes(side) {
  const pd = document.getElementById(side);
  const checks = pd.getElementsByTagName('input');
  for(let i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      checks[i].checked = false;
    }
  }
}

function copyList(side) {
  let string = '';
  const pd = document.getElementById(side);
  const checks = pd.getElementsByTagName('input');
  let fid;
  let datestr;
  let name;
  const sitedata = (side==='left')?lsite:rsite;
  const byfids = {};
  let size,user,group;
  for(let i = 0; i < sitedata.listing.length; i++) {
    byfids[sitedata.listing[i]['FID']] = sitedata.listing[i];
  }

  for(let i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      fid = checks[i].name.split('#',2)[1];
      if(fid === '-1') {
        continue;
      }

      size = byfids[fid]['SIZE'];
      size = Array(12 + 1 - size.length).join(' ') + size;
      user = byfids[fid]['USER'];
      user = Array(12 + 1 - user.length).join(' ') + user;
      group = byfids[fid]['GROUP'];
      group = Array(12 + 1 - group.length).join(' ') + group;

      datestr = ('DATE' in byfids[fid]) ? time2str(byfids[fid]['DATE']) : '';
      name = decode(byfids[fid]['NAME']);
      string += byfids[fid]['PERM'] + ' ' +
                user +
                ' ' +
                group +
                ' ' + size + ' ' + datestr + ' ' + name + '\n';
    }
  }
  //alert(string);
  const myWindow = window.open('', 'PDF', 'scrollbars=1;resizable=1;width=600;height=800');
  //myWindow = window.open("about:blank", "replace", "");
  //myWindow = window.open("data:text/html;charset=utf-8,<HTML><BODY><span id='fun'>test</span></body></html>", "tescticles", "");

  myWindow.document.write('<pre>\n'+string+'</pre>');
  //myWindow.document.body.innerHTML('<pre>\n'+string+'</pre>');

  myWindow.document.close();
  //myWindow.focus();
  //var doc=myWindow.document.open("text/html");
  //var pd=myWindow.getElementById("fun");
  //pd.innerHTML = "replaced test with something";
  //doc.close();
  //myWindow.focus();
  //myWindow.close();
}

function doCmdChange(side) {
  if(!connected) {
    return;
  }
  const site = (side==='left')?lsite:rsite;
  const pd = document.getElementById(side+'cmd');
  const input = document.getElementById(side+'cmdinput');
  // check for a saved command
  const savedcommand = pd.options[pd.selectedIndex].getAttribute('savedcmd');
  if(savedcommand) {
    document.getElementById(side+'cmdinput').value = savedcommand;
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

function defaultSite() {
  const site = {};

  site['NAME'] = '';
  site['HOST'] = '';
  site['PORT'] = '';
  site['USER'] = '';
  site['PASS'] = '';
  site['STARTDIR'] = '';
  site['SVTYPE'] = '';
  site['PASSIVE'] = '2';
  site['FXP_PASSIVE'] = '2';
  site['CONTROL_TLS'] = '2';
  site['DATA_TLS'] = '2';
  site['FXP_TLS'] = '2';
  site['IMPLICIT_TLS'] = '2';
  site['RESUME'] = '2';
  site['RESUME_LAST'] = '2';
  site['PRET'] = '2';
  site['USE_XDUPE'] = '2';
  site['FMOVEFIRST'] = '';
  site['FSKIPLIST'] = '';
  site['DMOVEFIRST'] = '';
  site['DSKIPLIST'] = '';
  site['FSKIPEMPTY'] = '2';
  site['DSKIPEMPTY'] = '2';
  site['SVDCMDS'] = '';
  site['RDIRS'] = '';

  return site;
}

function getExtraPairs(site) {
  let str='';

  for(const key in site) {
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
      str += '|'+key+'='+site[key];
    }
  }
  return str;
}

function doSMSiteUpdate() {
}

function doSMSiteChange() {
  // flush saved commands data and menu
  encodedsitecommands = '';
  sitesavedcommands.length = 0;
  while(saved_commands.childNodes.length > 0) {
    saved_commands.removeChild(saved_commands.lastChild);
  }
  document.getElementById('sv_newcmdargs').value = '';

  const smsites = document.getElementById('sm_sites');
  let s = smsites.value; // SITEID
  if(s === '-1') {
    site = defaultSite();
    site['FMOVEFIRST'] = '*.sfv/*.nfo';
    site['DMOVEFIRST'] = 'Sample';
    site['FSKIPLIST'] = '*COMPLETE*/*NUKE*/.message/.rules';
    site['DSKIPLIST'] = '*COMPLETE*/*NUKE*';
  } else {
    site = sitelist[s];
  }

  document.getElementById('sm_name').value = site['NAME'];
  document.getElementById('sm_host').value = site['HOST'];
  document.getElementById('sm_port').value = site['PORT'];
  document.getElementById('sm_user').value = site['USER'];
  document.getElementById('sm_pass').value = site['PASS'];
  document.getElementById('sm_startdir').value = 'STARTDIR' in site ? site['STARTDIR'] : '';
  document.getElementById('sm_svtype').value = site['SVTYPE'];
  document.getElementById('sm_pasv').value = site['PASSIVE'];
  document.getElementById('sm_fxpp').value = site['FXP_PASSIVE'];
  document.getElementById('sm_cssl').value = site['CONTROL_TLS'];
  document.getElementById('sm_dssl').value = site['DATA_TLS'];
  document.getElementById('sm_xssl').value = 'FXP_TLS' in site ? site['FXP_TLS'] : '2';
  document.getElementById('sm_implicit').value = 'IMPLICIT_TLS' in site ? site['IMPLICIT_TLS'] : '2';
  document.getElementById('sm_resume').value = 'RESUME' in site ? site['RESUME'] : '2';
  document.getElementById('sm_reslast').value = 'RESUME_LAST' in site ? site['RESUME_LAST'] : '2';
  document.getElementById('sm_pret').value = 'PRET' in site ? site['PRET'] : '2';
  document.getElementById('sm_xdupe').value = 'USE_XDUPE' in site ? site['USE_XDUPE'] : '2';
  document.getElementById('sm_fmove').value = 'FMOVEFIRST' in site ? site['FMOVEFIRST'] : '';
  document.getElementById('sm_fskip').value = 'FSKIPLIST' in site ? site['FSKIPLIST'] : '';
  document.getElementById('sm_dmove').value = 'DMOVEFIRST' in site ? site['DMOVEFIRST'] : '';
  document.getElementById('sm_dskip').value = 'DSKIPLIST' in site ? site['DSKIPLIST'] : '';
  document.getElementById('sm_fempty').value = 'FSKIPEMPTY' in site ? site['FSKIPEMPTY'] : '2';
  document.getElementById('sm_dempty').value = 'DSKIPEMPTY' in site ? site['DSKIPEMPTY'] : '2';

  s = getExtraPairs(site);
  document.getElementById('sm_extra').value = s.replace(/^\|/, '');

  if(site['HOST'] === '<local>') {
    document.getElementById('sm_local').checked = true;
  } else {
    document.getElementById('sm_local').checked = false;
  }
  const svtype = document.getElementById('sm_svtype').value;
  doLoadSiteCommands(svtype);
}

function smLocalChange() {
  if(document.getElementById('sm_local').checked === true) {
    document.getElementById('sm_host').value = '<local>';
    document.getElementById('sm_host').disabled = true;
    document.getElementById('sm_port').disabled = true;
    document.getElementById('sm_user').disabled = true;
    document.getElementById('sm_pass').disabled = true;
  } else {
    document.getElementById('sm_host').value = '';
    document.getElementById('sm_pass').disabled = false;
    document.getElementById('sm_user').disabled = false;
    document.getElementById('sm_port').disabled = false;
    document.getElementById('sm_host').disabled = false;
  }
}

function smSaveSite() {
  let save;
  const name = document.getElementById('sm_name').value;
  let site;
  const plain = defaultSite();

  WriteLog("Saving '"+name+"'.");
  for(const key in sitelist) {
    if(name === sitelist[key]['NAME']) {
      site = sitelist[key];
      save = 'SITEMOD|SITEID='+key;
      break;
    }
  }

  if(site === undefined) {
    site = plain;
    save = 'SITEADD';
  }

  if(document.getElementById('sm_local').checked) {
    document.getElementById('sm_host').value = '<local>';
    document.getElementById('sm_port').value = '21';
    document.getElementById('sm_user').value = 'NA';
    document.getElementById('sm_pass').value = 'NA';
  }

  // current site commands
  encodedsitecommands = b64wrap(sitesavedcommands,'encode');

  let val;
  if((val = document.getElementById('sm_name').value) !==
        (('NAME' in site) ? site['NAME'] : plain['NAME'])) {
    save += '|NAME='+val;
  }
  if((val = document.getElementById('sm_host').value) !==
        (('HOST' in site) ? site['HOST'] : plain['HOST'])) {
    save += '|HOST='+val;
  }
  if((val = document.getElementById('sm_port').value) !==
        (('PORT' in site) ? site['PORT'] : plain['PORT'])) {
    save += '|PORT='+val;
  }
  if((val = document.getElementById('sm_user').value) !==
        (('USER' in site) ? site['USER'] : plain['USER'])) {
    save += '|USER='+val;
  }
  if((val = document.getElementById('sm_pass').value) !==
        (('PASS' in site) ? site['PASS'] : plain['PASS'])) {
    save += '|PASS='+val;
  }
  if((val = document.getElementById('sm_svtype').value) !==
        (('SVTYPE' in site) ? site['SVTYPE'] : plain['SVTYPE'])) {
    save += '|SVTYPE='+val;
  }
  if((val = document.getElementById('sm_startdir').value) !==
        (('STARTDIR' in site) ? site['STARTDIR'] : plain['STARTDIR'])) {
    save += '|STARTDIR='+val;
  }
  if((val = document.getElementById('sm_pasv').value) !==
        (('PASSIVE' in site) ? site['PASSIVE'] : plain['PASSIVE'])) {
    save += '|PASSIVE='+val;
  }
  if((val = document.getElementById('sm_fxpp').value) !==
        (('FXP_PASSIVE' in site) ? site['FXP_PASSIVE'] : plain['FXP_PASSIVE'])) {
    save += '|FXP_PASSIVE='+val;
  }
  if((val = document.getElementById('sm_cssl').value) !==
        (('CONTROL_TLS' in site) ? site['CONTROL_TLS'] : plain['CONTROL_TLS'])) {
    save += '|CONTROL_TLS='+val;
  }
  if((val = document.getElementById('sm_dssl').value) !==
        (('DATA_TLS' in site) ? site['DATA_TLS'] : plain['DATA_TLS'])) {
    save += '|DATA_TLS='+val;
  }
  if((val = document.getElementById('sm_xssl').value) !==
        (('FXP_TLS' in site) ? site['FXP_TLS'] : plain['FXP_TLS'])) {
    save += '|FXP_TLS='+val;
  }
  if((val = document.getElementById('sm_implicit').value) !==
        (('IMPLICIT_TLS' in site) ? site['IMPLICIT_TLS'] : plain['IMPLICIT_TLS'])) {
    save += '|IMPLICIT_TLS='+val;
  }
  if((val = document.getElementById('sm_resume').value) !==
        (('RESUME' in site) ? site['RESUME'] : plain['RESUME'])) {
    save += '|RESUME='+val;
  }
  if((val = document.getElementById('sm_reslast').value) !==
        (('RESUME_LAST' in site) ? site['RESUME_LAST'] : plain['RESUME_LAST'])) {
    save += '|RESUME_LAST='+val;
  }
  if((val = document.getElementById('sm_pret').value) !==
        (('PRET' in site) ? site['PRET'] : plain['PRET'])) {
    save += '|PRET='+val;
  }
  if((val = document.getElementById('sm_xdupe').value) !==
        (('USE_XDUPE' in site) ? site['USE_XDUPE'] : plain['USE_XDUPE'])) {
    save += '|USE_XDUPE='+val;
  }
  if((val = document.getElementById('sm_fmove').value) !==
        (('FMOVEFIRST' in site) ? site['FMOVEFIRST'] : plain['FMOVEFIRST'])) {
    save += '|FMOVEFIRST='+val;
  }
  if((val = document.getElementById('sm_fskip').value) !==
        (('FSKIPLIST' in site) ? site['FSKIPLIST'] : plain['FSKIPLIST'])) {
    save += '|FSKIPLIST='+val;
  }
  if((val = document.getElementById('sm_dmove').value) !==
        (('DMOVEFIRST' in site) ? site['DMOVEFIRST'] : plain['DMOVEFIRST'])) {
    save += '|DMOVEFIRST='+val;
  }
  if((val = document.getElementById('sm_dskip').value) !==
        (('DSKIPLIST' in site) ? site['DSKIPLIST'] : plain['DSKIPLIST'])) {
    save += '|DSKIPLIST='+val;
  }
  if((val = document.getElementById('sm_fempty').value) !==
        (('FSKIPEMPTY' in site) ? site['FSKIPEMPTY'] : plain['FSKIPEMPTY'])) {
    save += '|FSKIPEMPTY='+val;
  }
  if((val = document.getElementById('sm_dempty').value) !==
        (('DSKIPEMPTY' in site) ? site['DSKIPEMPTY'] : plain['DSKIPEMPTY'])) {
    save += '|DSKIPEMPTY='+val;
  }
  if((val = encodedsitecommands) !==
        (('SVDCMDS' in site) ? site['SVDCMDS'] : plain['SVDCMDS'])) {
    if(typeof val !== 'undefined') {
      save += '|SVDCMDS='+val;
    }
  }
  if((val = document.getElementById('sm_extra').value) !==
        getExtraPairs(site)) {
    save += '|'+val;
  }

  // Send SITEADD or SITEMOD
  socket.send(save+'\n');
}

function smDeleteSite() {
  const smsites = document.getElementById('sm_sites');
  const s = smsites.value; // SITEID
  if(s === '-1') {
    return;
  }
  socket.send('SITEDEL|SITEID='+s+'\n');
}

function doCommand(side) {
  if(!connected) {
    return;
  }
  const site = (side==='left')?lsite:rsite;
  // if (site.session == '-1') return;
  const pd = document.getElementById(side+'cmd');
  const input = document.getElementById(side+'cmdinput');
  switch(pd.value) {
  case 'refresh':
    WriteLog('Refreshing directory');
    site.filter = null;
    doDIRLIST(site.session);
    break;
  case 'filter':
    WriteLog('Refreshing directory');
    site.filter = input.value;
    doDIRLIST(site.session);
    break;
  case 'selectall':
    setAllCheckboxes(side);
    break;
  case 'clearall':
    clearAllCheckboxes(side);
    break;
  case 'site':
    doSITECMD(site.session, input.value);
    break;
  case 'mkdir':
    doMKDCMD(site.session, input.value);
    break;
  case 'unlink':
    doUNLINKCMD(site.session, (side==='left')?'$<':'$>');
    break;
  case 'rename':
    doRENCMD(site.session, input.value);
    break;
  case 'compare':
    if((queue.qid === '-1') || (lsite.session === '-1') || (rsite.session === '-1')) {
      WriteLog('Both sites need to be connected for COMPARE');
      return;
    }
    doCOMPARE(queue.qid);
    break;
  case 'copylist':
    copyList(side);
    break;
  default:
    WriteLog('Not yet implemented, jefe');
  }
}

// all functions that execute an action on the engine with corresponding handler
function doSITELIST() {
  socket.send('SITELIST\n');
}
function doSESSIONNEW(side) {
  if(connected) {
    const site = (side==='left')?lsite:rsite;
    if(site.session !== '-1') {
      WriteLog('Site '+sitelist[site.siteid]['NAME']+' already connected!');
    } else {
      const pd = document.getElementById(side.substr(0,1)+'slist');
      site.siteid = pd.options[pd.selectedIndex].value;

      //put the textbox back
      let sd;
      if(side==='left') {
        sd = 'l';
      } else {
        sd = 'r';
      }
      const select = document.getElementById(sd+'dirselect');
      select.style.display = 'none';
      const textbox = document.getElementById(sd+'path');
      textbox.style.display = 'inline';
      textbox.value = '';

      // wipe filter from last session
      site.filter = null;

      // new session means new side+siderecentdirs
      const recentdirs = (side==='left')?leftsiderecentdirs:rightsiderecentdirs;
      recentdirs.length=0;

      // get recent dirs, sets siterecentdirs
      svCmdReadWrite('read','RDIRS',site.siteid);

      // populate recent directories
      if(siterecentdirs.length > 1) {
        if(side === 'left') {
          leftsiderecentdirs = siterecentdirs.slice();
        } else if(side === 'right') {
          rightsiderecentdirs = siterecentdirs.slice();
        }
        // flush for next use
        siterecentdirs.length=0;
      }

      // populate top menu saved commands when we (try to) connect
      // read from object into tmp so sitesavedcmds doesn't break
      const tmptopcmdmenu = svCmdReadWrite('read','SVDCMDS',site.siteid);
      if(tmptopcmdmenu.length > 1) {
        const tmptopmenu = svCmdReadWrite('read','SVDCMDS',site.siteid).slice();
        for(let i = 0; i < tmptopmenu.length; i++) {
          svCmdTopMenu(tmptopmenu,side);
        }
      }

      // LOG=1 needed for LOG command
      socket.send('SESSIONNEW|SITEID='+site.siteid+'|LOG=1\n');
      dirlistStatus(side,'**CONNECTING**');
    }
  } else {
    WriteLog('Not connected!');
  }
}
function doSESSIONFREE(side) {
  if(connected) {
    const site = (side==='left')?lsite:rsite;
    // if site.session isn't defined we're not connected at all
    if(site.session === '-1') {
      WriteLog('Not connected to a site!');
    } else {
      if(queue.qid !== '-1') {
        doQUEUEFREE(queue.qid);
      }

      //put the textbox back
      let sd;
      if(side==='left') {
        sd = 'l';
      } else {
        sd = 'r';
      }
      const select = document.getElementById(sd+'dirselect');
      select.style.display = 'none';
      const textbox = document.getElementById(sd+'path');
      textbox.style.display = 'inline';
      textbox.value = '';
      //clear the site cmd box
      const cmdbox = document.getElementById(side+'cmdinput');
      cmdbox.value = '';

      socket.send('SESSIONFREE|SID='+site.session+'\n');
    }
  } else {
    WriteLog('Not connected!');
  }
}
function doQUEUENEW() {
  if(connected) {
    if(queuenewlock === 0) {
      queuenewlock = 1;
      socket.send('QUEUENEW|NORTH_SID='+lsite.session+'|SOUTH_SID='+rsite.session+'\n');
    }
  }
}
function doQUEUEFREE(qid) {
  if(connected) {
    socket.send('QUEUEFREE|QID='+qid+'\n');
  }
}
function doCWD(sid,dir) {
  if(connected) {
    socket.send('CWD|SID='+sid+'|PATH='+dir+'\n');
  }
}
function doPWD(sid) {
  if(connected) {
    socket.send('PWD|SID='+sid+'\n');
  }
}
function doDIRLIST(sid) {
  let side;
  if(sid === lsite.session) {
    side='left';
  } else if(sid === rsite.session) {
    side='right';
  }
  if(connected) {
    clearTable(side);
    socket.send('DIRLIST|SID='+sid+'\n');
  }
}

function doGO() {
  if(queue.qid === '-1') {
    return;
  }

  // session is going to drop, save rdirs on both sides
  const sitel = lsite;
  if(sitel.session > 0) {
    svCmdReadWrite('write','RDIRS',sitel.siteid);
  }
  const siter = rsite;
  if(siter.session > 0) {
    svCmdReadWrite('write','RDIRS',siter.siteid);
  }

  socket.send('GO|SUBSCRIBE|QID='+queue.qid+'\n');
}

function doSTOP() {
  if(queue.qid === '-1') {
    return;
  }
  socket.send('STOP|QID='+queue.qid+'\n');
}

function doQITOP() {
  if(queue.qid === '-1') {
    return;
  }
  const pd = document.getElementById('queue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 1; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      socket.send('QMOVE|QID='+queue.qid+'|FROM='+i+'|TO=FIRST\n');
    }
  }
}
function doQIBOTTOM() {
  if(queue.qid === '-1') {
    return;
  }
  const pd = document.getElementById('queue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 0; i < checks.length-1; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      socket.send('QMOVE|QID='+queue.qid+'|FROM='+i+'|TO=LAST\n');
    }
  }
}
function doQIUP() {
  if(queue.qid === '-1') {
    return;
  }
  const pd = document.getElementById('queue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 1; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      socket.send('QMOVE|QID='+queue.qid+'|FROM='+i+'|TO='+(i-1)+'\n');
    }
  }
}
function doQIDOWN() {
  if(queue.qid === '-1') {
    return;
  }
  const pd = document.getElementById('queue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 0; i < checks.length-1; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      socket.send('QMOVE|QID='+queue.qid+'|FROM='+i+'|TO='+(i+1)+'\n');
    }
  }
}
function doQIDELETE() {
  if(queue.qid === '-1') {
    return;
  }
  const pd = document.getElementById('queue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = checks.length-1; i >= 0; i--) { // reverse
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      socket.send('QDEL|QID='+queue.qid+'|@='+i+'\n');
    }
  }
}
function doQICLEAR() {
  if(queue.qid === '-1') {
    return;
  }
  doQUEUEFREE(queue.qid);
  doQUEUENEW();
}
function doQISELECT() {
  const pd = document.getElementById('queue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === false) {
      checks[i].checked = true;
    }
  }
}
function doQIDESELECT() {
  if(queue.qid === '-1') {
    return;
  }
  const pd = document.getElementById('queue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      checks[i].checked = false;
    }
  }
}
function doQIINSERT() {
}

// Execute CMD. If CMD contain $< or $> iterate left,right selected items.
function expandInput(cmd, onlyfiles, onlydirs, original) {
  const reply = [];
  let offset;
  let re;
  let side;

  if((offset = cmd.indexOf('$<')) >= 0) { // iterate left
    side = 'left';
    re = /\$</gi;
  } else if((offset = cmd.indexOf('$>')) >= 0) { // iterate right
    side = 'right';
    re = /\$>/gi;
  } else {
    reply.push(cmd);
    return reply;
  }

  // Iterate side, build array with replacements.
  const pd = document.getElementById(side);
  const checks = pd.getElementsByTagName('input');
  const sitedata = (side==='left')?lsite:rsite;
  const byfids = {};
  for(let i = 0; i < sitedata.listing.length; i++) {
    byfids[sitedata.listing[i]['FID']] = sitedata.listing[i];
  }
  for(let i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      const fid = checks[i].name.split('#',2)[1];
      if(onlydirs === true) {
        if(byfids[fid]['FTYPE'].toLowerCase() !== 'directory') {
          continue;
        }
      }
      if(onlyfiles === true) {
        if(byfids[fid]['FTYPE'].toLowerCase() === 'directory') {
          continue;
        }
      }
      reply.push(cmd.replace(re, byfids[fid]['NAME']));
      if(original) {
        original.push(byfids[fid]['NAME']);
      }
    }
  }
  return reply;
}

function doSITECMD(sid,cmd) {
  const list = expandInput(cmd);
  for(let i = 0; i < list.length; i++) {
    socket.send('SITE|SID='+sid+'|LOG|CMD='+encode(list[i])+'\n');
  }
}

function doMKDCMD(sid,cmd) {
  const list = expandInput(cmd);
  for(let i = 0; i < list.length; i++) {
    socket.send('MKD|SID='+sid+'|PATH='+encode(list[i])+'\n');
  }
  doDIRLIST(sid);
}

function doUNLINKCMD(sid,cmd) {
  // We need to split files and dirs, and delete separately.
  let list = expandInput(cmd, true, false);
  for(let i = 0; i < list.length; i++) {
    socket.send('DELE|SID='+sid+'|PATH='+encode(list[i])+'\n');
  }
  list = expandInput(cmd, false, true);
  for(let i = 0; i < list.length; i++) {
    socket.send('RMD|SID='+sid+'|PATH='+encode(list[i])+'\n');
  }
  doDIRLIST(sid);
}

function doRENCMD(sid,cmd) {
  const original = [];
  let list;

  // Special case, if no pattern is specified, we allow direct rename from
  // one selected item (first selected) to literal input string.
  if((cmd.indexOf('$<') === -1) &&
        (cmd.indexOf('$>') === -1)) {
    list = expandInput((sid===lsite.session)?'$<':'$>', false, false, original);
    list.length = 1; // First one only
    original.length = 1;
    original[0] = list[0];
    list[0] = cmd; // Remove the "expanded" name
  } else {
    list = expandInput(cmd, false, false, original);
  }

  for(let i = 0; i < list.length; i++) {
    socket.send('REN|SID='+sid+'|FROM='+encode(original[i])+'|TO='+encode(list[i])+'\n');
  }
  doDIRLIST(sid);
}

function doCOMPARE(qid) {
  socket.send('QCOMPARE|QID='+qid+'\n');
}

// *** QueueManager

function doQMRefresh() {
  socket.send('QLIST\n');
}

function doQMClearIdle() {
  for(let i = 0; i < queuemanager.listing.length; i++) {
    if((queuemanager.listing[i]['STATUS'] === 'IDLE') &&
            (queuemanager.listing[i]['QID'] !== queue.qid)) {
      socket.send('QUEUEFREE|QID='+queuemanager.listing[i]['QID']+'\n');
    }
  }
  doQMRefresh();
}

function doQMNuke() {
  const pd = document.getElementById('qmqueue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      const qid = checks[i].name.split('#',2)[1];
      if(qid === queue.qid) {
        WriteLog("Releasing this UI's queue will make bad things happen.");
      }
      socket.send('QUEUEFREE|QID='+qid+'\n');
    }
  }
  doQMRefresh();
}

function doQMClone() {
  const pd = document.getElementById('qmqueue');
  const checks = pd.getElementsByTagName('input');
  let i;
  for(i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      const qid = checks[i].name.split('#',2)[1];
      socket.send('QCLONE|QID='+qid+'\n');
    }
  }
  doQMRefresh();
}

function doQMSubscribe() {
  const pd = document.getElementById('qmqueue');
  const checks = pd.getElementsByTagName('input');
  let i;
  let sent=0;
  for(i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      const qid = checks[i].name.split('#',2)[1];
      socket.send('SUBSCRIBE|TOGGLE|QID='+qid+'\n');
      sent++;
    }
  }
  if(sent) {
    doQMRefresh();
  }
}

function doQMGrab() {
  const pd = document.getElementById('qmqueue');
  const checks = pd.getElementsByTagName('input');
  let i;
  const sent=0;
  for(i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      const qid = checks[i].name.split('#',2)[1];
      // GRAB queue, and stop. We only grab the first ticked queue
      socket.send('QGRAB|QID='+qid+'\n');
      return;
    }
  }
}

function doQGET(qid) {
  socket.send('QGET|QID='+qid+'\n');
}

function doQMView() {
  const pd = document.getElementById('qmqueue');
  const checks = pd.getElementsByTagName('input');
  let i;
  let sent=0;
  for(i = 0; i < checks.length; i++) {
    if(checks[i].type === 'checkbox' && checks[i].checked === true) {
      const qid = checks[i].name.split('#',2)[1];
      socket.send('QERR|QID='+qid+'\n');
      sent++;
    }
  }
  if(sent) {
    doQMRefresh();
  }
}

function doEnterKey() {
  const focusbaboon = document.activeElement.id;
  event.preventDefault();
  if(event.keyCode === 13) {
    switch(focusbaboon) {
    case 'leftcmdinput':
      document.getElementById('leftcommand').click();
      break;
    case 'rightcmdinput':
      document.getElementById('rightcommand').click();
      break;
    }
  }
}

function doLogClear(clickside) {
  if(clickside === 'left') {
    document.getElementById('lwalloutput').innerHTML = '';
  } else if(clickside === 'right') {
    document.getElementById('rwalloutput').innerHTML = '';
  }
}

// all the code for the saved commands editor
function doLoadSiteCommands(svtype) {
  // reset warning messages
  document.getElementById('savedcmdeditoroutput').innerHTML = '';

  // Standard commands from site cmd menu
  let specialcmds = '';
  // only commands that take arguments
  const standardcmds = ['filter','site cmd','new dir','delete','rename'];

  // FXPOne site commands via 'help'
  const fxponecmds = ['-FXPOne Commands-','adduser','chgrp','chown','colour','deluser','dupe','extra','give','groupadd',
    'groupdel','groupinfo','grouplist','groupuser','gtagline','gtopdn','gtopup','help','incompletes',
    'kick','move','msg','new','nuke','passwd','pre','races','rehash','renuser','reqfilled','request',
    'search','section','setcred','setflags','setip','setlimit','setpass','setratio','tagline','tcpstat',
    'topdn','topup','unnuke','user','wall','who'];

  // add the other more inferior ftp server types here later
  if(svtype === 'fxpone') {
    specialcmds = fxponecmds;
  }

  // clear menu first
  while(sv_newcmd.childNodes.length > 0) {
    sv_newcmd.removeChild(sv_newcmd.lastChild);
  }
  // add defaults
  for(let i = 0; i < standardcmds.length; i++) {
    const option = document.createElement('option');
    option.value = standardcmds[i];
    option.text = standardcmds[i];
    if(option.value === 'site cmd') {
      option.value = 'site';
    } else if(option.text === 'site cmd') {
      option.text = 'site';
    }
    sv_newcmd.appendChild(option);
  }
  // now the 'specials"
  for(let i = 0; i < specialcmds.length; i++) {
    const option = document.createElement('option');
    option.value = 'site '+specialcmds[i];
    option.text = specialcmds[i];
    if(option.value === 'site -FXPOne Commands-') {
      option.disabled = true;
      option.text = '-FXPOne Commands-';
    }
    sv_newcmd.appendChild(option);
  }

  // check SVDCMDS value for commands
  const savedCmdsData = site['SVDCMDS'];
  if(savedCmdsData && savedCmdsData.length > 0) {
    sitesavedcommands = svCmdReadWrite('read','SVDCMDS',site['SITEID']).slice();
    svRedrawCmdGUI();
  }
}

function svNewCommand() {
  // pick a site first, friend
  document.getElementById('savedcmdeditoroutput').innerHTML = '';
  if(document.getElementById('sm_sites').value === '-1') {
    document.getElementById('savedcmdeditoroutput').innerHTML = 'Please create or select a site first.';
    return;
  }
  document.getElementById('savedcmdeditoroutput').innerHTML = '';
  let newWriteCmd;
  const newCommand = document.getElementById('sv_newcmd').value;
  const newArgs = document.getElementById('sv_newcmdargs').value.trim();
  //check for empty values
  if(!newCommand) {
    document.getElementById('savedcmdeditoroutput').innerHTML = 'Missing command. Please try again.';
    return;
  }
  // empty arguments
  if(!newArgs.replace(/\s/g, '').length) {
    newWriteCmd = newCommand;
  } else {
    newWriteCmd = newCommand.concat(' ',newArgs);
  }
  // check for existing command
  const cmdMatch = sitesavedcommands.indexOf(newWriteCmd);
  if(cmdMatch !== -1) {
    document.getElementById('savedcmdeditoroutput').innerHTML = 'Command already exists. Please try again.';
  } else {
    // new command, push to sitesavedcommands array
    sitesavedcommands.push(newWriteCmd);

    // Write out to the site data, redraw gui
    svCmdReadWrite('write','SVDCMDS',site['SITEID']);
    svRedrawCmdGUI();
  }
}

// future use
function svCmdArrayUpdate(sitesavedcommands) {
  // try to avoid wiping it all
  if(sitesavedcommands.length < 1) {
    WriteLog('Fatal error with saved commands data; aborting.');
  }
}

function svCmdTopMenu(tmptopmenu,side) {
  const cmdmenu = document.getElementById(side+'cmd');
  const site = (side==='left')?lsite:rsite;
  if(tmptopmenu) {
    // local variable so editing doesn't break the menu
    const menusavedcommands = tmptopmenu.slice();
    // clear the menu
    while(cmdmenu.childNodes.length > 0) {
      cmdmenu.removeChild(cmdmenu.lastChild);
    }
    // repopulate with default + saved
    const staticcmds = ['refresh|refresh','filter|filter',
      'selectall|select all','clearall|clear all','site|site cmd',
      'mkdir|new dir','unlink|delete','rename|rename','compare|compare',
      'copylist|copy list'];
    for(let i = 0; i < staticcmds.length; i++) {
      const option = document.createElement('option');
      option.value = staticcmds[i].split('|')[0];
      if(option.value === 'site') {
        option.selected = true;
      }
      option.text = staticcmds[i].split('|')[1];
      cmdmenu.appendChild(option);
    }
    // add saved commands
    // site name first as divider
    const option = document.createElement('option');
    option.value = 'separator'
    option.text = '-'+sitelist[site.siteid]['NAME']+'-';
    option.disabled = true;
    cmdmenu.appendChild(option);
    // saved commands, use custom savecmd tag for args in doCommand
    for(let i = 0; i < menusavedcommands.length; i++) {
      const option = document.createElement('option');
      option.value = menusavedcommands[i].split(' ')[0];
      option.text = menusavedcommands[i];
      const tmpcmd = menusavedcommands[i];
      const savedcmdstr = tmpcmd.substr(tmpcmd.indexOf(' ') + 1);
      option.setAttribute('savedcmd',savedcmdstr);
      option.text = menusavedcommands[i];
      cmdmenu.appendChild(option);
    }
  }
}

function svCmdReadWrite(rwcommand,type,siteid) {
  const site = sitelist[siteid];
  let tmpsavedcmds;
  let encodeddata = '';
  let side;
  if(siteid === lsite.siteid) {
    side = 'left';
  } else if(siteid === rsite.siteid) {
    side = 'right';
  }

  if(rwcommand === 'read') {
    encodeddata = site[type];
    if(!encodeddata || encodeddata === null || encodeddata === '') {
      // WriteLog("Cannot read " + type + " from " + sitelist[siteid]["NAME"]);
      return -1;
    }
    if(type === 'SVDCMDS') {
      tmpsavedcmds = b64wrap(encodeddata,'decode');
      if(tmpsavedcmds) {
        const tmpsitesavedcommands = tmpsavedcmds.split(',');
        return tmpsitesavedcommands;
      } else {
        return -1;
      }
    } else if(type === 'RDIRS') {
      tmpsavedcmds = b64wrap(encodeddata,'decode');
      if(tmpsavedcmds) {
        siterecentdirs = tmpsavedcmds.split(',');
      } else {
        return -1;
      }
    }
  } else if(rwcommand === 'write') {
    if(type === 'SVDCMDS') {
      encodedsitecommands = b64wrap(sitesavedcommands,'encode');
      if(encodedsitecommands) {
        const save = 'SITEMOD|SITEID=' + siteid + '|SVDCMDS=' + encodedsitecommands;
        socket.send(save+'\n');
        // update the site{} data
        sitelist[siteid]['SVDCMDS'] = encodedsitecommands;
        // Also update top level menus if site matches
        if(side) {
          svCmdTopMenu(sitesavedcommands,side);
        }
      }
    }
    if(type === 'RDIRS') {
      tmpsavedcmds = (side==='left')?leftsiderecentdirs.toString():rightsiderecentdirs.toString();
      const tmpencodedrecentdirs = b64wrap(tmpsavedcmds,'encode');
      let save;
      if(tmpencodedrecentdirs) {
        save = 'SITEMOD|SITEID=' + siteid + '|RDIRS=' + tmpencodedrecentdirs;
      } else {
        save = 'SITEMOD|SITEID=' + siteid;
      }
      // WriteLog("Saved rdir site data: " + tmpsavedcmds);
      socket.send(save+'\n');
    }
  }
}

function svUpdateCommand(updateid) {
  sitesavedcommands[updateid] = document.getElementById('sv_args'+updateid).value;
  svCmdReadWrite('write','SVDCMDS',site['SITEID']);
}

function svDeleteCommand(delid) {
  sitesavedcommands.splice(delid, 1);
  svRedrawCmdGUI();
  svCmdReadWrite('write','SVDCMDS',site['SITEID']);
}

function svMoveCommand(arrayid,direction) {
  switch(direction) {
  case 'up':
    if(arrayid !== 0) {
      const ahead = +arrayid - 1;
      const aheadval = document.getElementById('sv_args'+ahead).value;
      sitesavedcommands[ahead] = document.getElementById('sv_args'+arrayid).value;
      sitesavedcommands[arrayid] = aheadval;
      svRedrawCmdGUI();
      svCmdReadWrite('write','SVDCMDS',site['SITEID']);
    }
    break;
  case 'down':
    if(arrayid !== sitesavedcommands.length) {
      const ahead = +arrayid + 1;
      const aheadval = document.getElementById('sv_args'+ahead).value;
      sitesavedcommands[ahead] = document.getElementById('sv_args'+arrayid).value;
      sitesavedcommands[arrayid] = aheadval;
      svRedrawCmdGUI();
      svCmdReadWrite('write','SVDCMDS',site['SITEID']);
    }
    break;
  case 'top':
    sitesavedcommands.splice(arrayid,1);
    sitesavedcommands.unshift(document.getElementById('sv_args'+arrayid).value);
    svRedrawCmdGUI();
    svCmdReadWrite('write','SVDCMDS',site['SITEID']);
    break;
  case 'bottom':
    sitesavedcommands.splice(arrayid,1);
    sitesavedcommands.push(document.getElementById('sv_args'+arrayid).value);
    svRedrawCmdGUI();
    svCmdReadWrite('write','SVDCMDS',site['SITEID']);
    break;
  default:
    break;
  }
}

function svRedrawCmdGUI() {
  while(saved_commands.childNodes.length > 0) {
    saved_commands.removeChild(saved_commands.lastChild);
  }

  for(let i = 0; i < sitesavedcommands.length; i++) {
    const cmd = sitesavedcommands[i];
    if(cmd == null) {
      return;
    }
    svDrawCmdGUI(cmd,i);
  }
}

function svDrawCmdGUI(command,arraynum) {
  // div wrapper
  const cmdwrapper = document.createElement('div');
  // images
  const imgsave = document.createElement('input');
  const imgup = document.createElement('input');
  const imgdelete = document.createElement('input');
  const imgdown = document.createElement('input');
  const imgtop = document.createElement('input');
  const imgbottom = document.createElement('input');

  cmdwrapper.type = 'div';
  cmdwrapper.id = 'cmdwrapper' + arraynum;
  cmdwrapper.title = 'Command wrapper' + arraynum;
  document.getElementById('saved_commands').appendChild(cmdwrapper);

  const cmdinput = document.createElement('input');
  cmdinput.type = 'text';
  cmdinput.id = 'sv_args' + arraynum;
  cmdinput.title = 'Command arguments';
  cmdinput.value = command;
  document.getElementById('cmdwrapper' + arraynum).appendChild(cmdinput);

  // images save,delete,up,down,top,bottom
  imgsave.type = 'image';
  imgsave.className = 'ctrls';
  imgsave.src = 'img/save.gif';
  imgsave.title = 'Update saved command';
  imgsave.id = arraynum;
  imgsave.setAttribute('onclick', 'svUpdateCommand(this.id);');
  document.getElementById('cmdwrapper' + arraynum).appendChild(imgsave);

  imgdelete.type = 'image';
  imgdelete.className = 'ctrls';
  imgdelete.src = 'img/delete_item.png';
  imgdelete.title = 'Delete saved command';
  imgdelete.id = arraynum;
  imgdelete.setAttribute('onclick', 'svDeleteCommand(this.id);');
  document.getElementById('cmdwrapper' + arraynum).appendChild(imgdelete);

  imgup.type = 'image';
  imgup.className = 'ctrls';
  imgup.src = 'img/move-up.png';
  imgup.title = 'Move up';
  imgup.id = arraynum;
  imgup.setAttribute('onclick', 'svMoveCommand(this.id,"up");');
  document.getElementById('cmdwrapper' + arraynum).appendChild(imgup);

  imgdown.type = 'image';
  imgdown.className = 'ctrls';
  imgdown.src = 'img/move-down.png';
  imgdown.title = 'Move down';
  imgdown.id = arraynum;
  imgdown.setAttribute('onclick', 'svMoveCommand(this.id,"down");');
  document.getElementById('cmdwrapper' + arraynum).appendChild(imgdown);

  imgtop.type = 'image';
  imgtop.className = 'ctrls';
  imgtop.src = 'img/move-top.png';
  imgtop.title = 'Move to top';
  imgtop.id = arraynum;
  imgtop.setAttribute('onclick', 'svMoveCommand(this.id,"top");');
  document.getElementById('cmdwrapper' + arraynum).appendChild(imgtop);

  imgbottom.type = 'image';
  imgbottom.className = 'ctrls';
  imgbottom.src = 'img/move-bottom.png';
  imgbottom.title = 'Move to bottom';
  imgbottom.id = arraynum;
  imgbottom.setAttribute('onclick', 'svMoveCommand(this.id,"bottom");');
  document.getElementById('cmdwrapper' + arraynum).appendChild(imgbottom);
}

function showRecentDirs(side,action) {
  let sd;
  let siteSession;
  let recentdirs;
  let dirselect;
  if(side === 'left') {
    sd = 'l';
    siteSession = lsite.session;
    recentdirs = leftsiderecentdirs;
    dirselect = ldirselect;
  } else {
    sd = 'r';
    siteSession = rsite.session;
    recentdirs = rightsiderecentdirs;
    dirselect = rdirselect;
  }

  // disable when not connected
  if(siteSession === '-1') {
    action = 'invalid';
  }

  if(action === 'invalid') {
    const select = document.getElementById(sd+'dirselect');
    select.style.display = 'none';
    const textbox = document.getElementById(sd+'path');
    textbox.style.display = 'inline';
    textbox.value = '';
    return;
  }

  if(action === 'show') {
    if(recentdirs.length === 0) {
      return;
    }
    const textbox = document.getElementById(sd+'path');
    textbox.style.display = 'none';
    const select = document.getElementById(sd+'dirselect');

    //wipe first
    while(select.childNodes.length > 0) {
      select.removeChild(select.lastChild);
    }
    //placeholder for going back
    const option = document.createElement('option');
    option.value = '--placeholder--';
    option.text = '-Recent directories-';
    dirselect.appendChild(option);

    select.style.display = 'inline';
    for(let i = 0; i < recentdirs.length; i++) {
      const option = document.createElement('option');
      option.value = recentdirs[i];
      option.text = recentdirs[i];
      dirselect.appendChild(option);
    }
    // deselect all so placeholder works
    document.getElementById(sd+'dirselect').selectedIndex = '-1';
  }
  if(action === 'hide') {
    const select = document.getElementById(sd+'dirselect');
    select.style.display = 'none';
    const textbox = document.getElementById(sd+'path');
    textbox.style.display = 'inline';
    const selecteddir = document.getElementById(sd+'dirselect').value;
    if(selecteddir === '--placeholder--') {
      return;
    }
    document.getElementById(sd+'path').value = selecteddir;
    doCWDEdit(side);
  }
}

function rememberCWD(side,dir) {
  const site = (side==='left')?lsite:rsite;
  const sitename = sitelist[site.siteid]['NAME'];
  const recentdirs = (side==='left')?leftsiderecentdirs:rightsiderecentdirs;
  if(dir === '..') {
    return;
  } else if(dir === sitelist[site.siteid]['STARTDIR']) {
    return;
  }
  // oft used dirs to top
  for(let i = 0; i < recentdirs.length; i++) {
    if(dir === recentdirs[i]) {
      recentdirs.splice(i,1);
    }
  }
  if(recentdirs.length < 10) {
    recentdirs.unshift(dir);
  } else {
    recentdirs.pop();
    recentdirs.unshift(dir);
  }
}
