// all the command handlers and their wrapping function
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

/* global doClose */
/* global doSITELIST */
/* global doSMSiteChange */
/* global doQueueWipe */
/* global doQUEUENEW */
/* global doPWD */
/* global doQUEUEFREE */
/* global doDIRLIST */
/* global doQGET */
/* global TextToAnsi */
/* global WriteLargeLog */
/* global WriteQMLog */
/* global clearAllCheckboxes */
/* global setCheckboxes */
/* global refreshTable */
/* global move */
/* global clearLog */
/* global svCmdReadWrite */
/* global rememberCWD */
/* global WriteLog */
/* global clearTable */
/* global decode */
/* global dirlistStatus */
/* global doCWD */
/* global time2str */
/* global toggleView */

function handlewrap(func) {
  if(this['handle'+func]) {
    this['handle'+func].apply(this, Array.prototype.slice.call(arguments, 1));
  } else {
    this['handleUNKNOWN'].apply(this, Array.prototype.slice.call(arguments, 1));
  }
}

function handleUNKNOWN(data) {
  WriteLog('Unknown reply '+data['func']);
  for(const flower in data) {
    if(data[flower]) {
      WriteLog('       ' + flower + ' = ' + data[flower]);
    } else {
      WriteLog('       ' + flower);
    }
  }
}
function handleWELCOME(data) {
  WriteLog(data['NAME']+' version '+data['VERSION']+' build '+data['BUILD']+' protocol '+data['PROTOCOL']+', SSL is '+data['SSL']);
}
function handleAUTH(data) {
  if(data['CODE'] !== '0') {
    const msg = 'Auth failed: ['+data['CODE']+']'+data['MSG']+', check credentials and try again.';
    WriteLog(msg);
    document.getElementById('connectstatus').innerHTML = msg;
    const butt = document.getElementById('connect');
    butt.value='Retry'; butt.alt='Retry'; butt.title='Retry';
    butt.src='img/failconnect.png';
    doClose();
  } else {
    WriteLog('Authorization '+data['MSG']+'.');
    document.getElementById('cblock').style.display = 'none';
    document.getElementById('connectstatus').innerHTML = "<input class='ctrls' type='image' alt='Password Change' title='Password Change' src='img/grab.png' onclick='togglePwdChange();' value='Change Password' id='pwdchange'>";
    localStorage.host=document.getElementById('host').value;
    localStorage.user=document.getElementById('user').value
    //localStorage.pass=document.getElementById("pass").value;
    connected = true;
    toggleView('primary');
    doSITELIST();
  }
}

function handleWHO(data) {
  if('BEGIN' in data) {
    WriteLog('User         HOST          SSL  (Total '+data['CLIENTS']+')');
  } else if('END' in data) {
    WriteLog('End of WHO');
  } else {
    WriteLog(data['NAME'] + '      ' + data['HOST'] + '    ' + data['SSL']);
  }
}
function handleSITELIST(data) {
  if('BEGIN' in data) {
    sitelist = {};
    const lsites = document.getElementById('lslist');
    const rsites = document.getElementById('rslist');

    if(lsites.length > 0) {
      const lsiteindex = lsites.selectedIndex;
      const rsiteindex = rsites.selectedIndex;
      selectedlsiteid = lsites[lsiteindex].value;
      selectedrsiteid = rsites[rsiteindex].value;
    }

    while(lsites.length> 0) {
      lsites.remove(0);
    }
    while(rsites.length> 0) {
      rsites.remove(0);
    }
    const smsites = document.getElementById('sm_sites');

    // save current site for loading later (saved commands editing)
    selectedSite = smsites.options[smsites.selectedIndex].value;

    while(smsites.length> 0) {
      smsites.remove(0);
    }
    return;
  }
  if('END' in data) { // gui shit here
    const lsites = document.getElementById('lslist');
    const rsites = document.getElementById('rslist');
    const smsites = document.getElementById('sm_sites');
    const sitenames = [];

    for(const key in sitelist) {
      sitenames.push([key, sitelist[key]['NAME']]);
    }
    sitenames.sort(function(a, b) {
      a = a[1];
      b = b[1];

      return a < b ? -1 : (a > b ? 1 : 0);
    });

    const opt = document.createElement('option');
    opt.text = '--New--';
    opt.value = '-1';
    smsites.add(opt,null);

    for(let i = 0; i < sitenames.length; i++) {
      const key = sitenames[i][0];
      const value = sitenames[i][1];

      const opt = document.createElement('option');
      opt.text = value;
      opt.value = key;
      lsites.add(opt,null);
      rsites.add(opt.cloneNode(true),null);
      smsites.add(opt.cloneNode(true),null);

      // remember the site we had selected so sitelist doesn't wipe it
      if(selectedlsiteid === key && firstload !== false) {
        lsites.selectedIndex = i;
      }
      if(selectedrsiteid === key && firstload !== false) {
        rsites.selectedIndex = i;
      }

      if(firstload === true) {
        if(localStorage.lastleftsite === key) {
          lsites.selectedIndex = i;
        }
        if(localStorage.lastrightsite === key) {
          rsites.selectedIndex = i;
        }
      }
    }
    // initial load of last used sites completed
    firstload = true;

    // we're working on this site, reload
    for(let i=0; i<smsites.options.length; i++) {
      if(smsites.options[i].value === selectedSite) {
        smsites.selectedIndex = i;
        break;
      }
    }
    doSMSiteChange();
    return;
  }
  sitelist[data['SITEID']] = data;
}
function handleSESSIONNEW(data) {
  if(lsite.session === '-1'&&rsite.session === '-1'&&queue.qid !== '-1') {   // did go, connecting fresh, clear queue
    queue.qid = '-1';
    doQueueWipe();  // gui queue wipe
  }
  if(lsite.siteid === data['SITEID'] && lsite.session === '-1') {
    lsite.session = data['SID'];
  } else if(rsite.siteid === data['SITEID'] && rsite.session === '-1') {
    rsite.session = data['SID'];
  } else {
    if(!debug) {
      WriteLog('Unknown sessionnew response!');
    }
  }
  if(data['SID'] === lsite.session) {
    const side='left';
    const site = (side==='left')?lsite:rsite;
    // clear the site log
    document.getElementById('lwalloutput').innerHTML = '';
    // update the name in the site log
    document.getElementById('lloglabel').innerHTML = '<input type="image" align="right" class="ctrls" src="img/delete_item.png" title="Clear log" onclick="doLogClear(\'left\');">';
    document.getElementById('lloglabel').innerHTML += '[ ' + sitelist[site.siteid]['NAME'] + ' ] site log';
  } else if(data['SID'] === rsite.session) {
    const side='right';
    const site = (side==='left')?lsite:rsite;
    // clear the site log
    document.getElementById('rwalloutput').innerHTML = '';
    // update the name in the site log
    document.getElementById('rloglabel').innerHTML = '<input type="image" align="right" class="ctrls" src="img/delete_item.png" title="Clear log" onclick="doLogClear(\'right\');">';
    document.getElementById('rloglabel').innerHTML += '[ ' + sitelist[site.siteid]['NAME'] + ' ] site log';
  }
}

function handleLOG(data) {
  // try to print without being too verbose
  let side = '';
  if(data['SID'] === lsite.session) {
    side='left';
  } else if(data['SID'] === rsite.session) {
    side='right';
  }
  const text = decode(data['MSG']);

  // literal text null showing up
  if(text === '(null)') {
    return;
  }

  // limited space on the main site log
  const pattern1 = /^\d{3}[^-]/;
  const pattern1match = pattern1.test(text);
  if(pattern1match) {
    // these all go to the main site log
    const mainlogmsg = /^(200|220|226|230|421|425|426|500|501|502|504|550)[^-]/;
    const matchedmainlog = mainlogmsg.test(text);
    if(matchedmainlog) {
      const ansitext = TextToAnsi(text);
      WriteLog(ansitext);
      WriteLargeLog(side,ansitext);
      return;
    } else {
      const ansitext = TextToAnsi(text);
      WriteLargeLog(side,ansitext);
      return;
    }
  }

  // drop first four characters for server messages with a trailing -
  const pattern = /^\d{3}-/;
  const cuttem = pattern.test(text);
  if(cuttem) {
    const newtext = text.slice(4);
    const ansitext = TextToAnsi(newtext);
    WriteLog(ansitext);
    WriteLargeLog(side,ansitext);
    return;
  }

  // The rest (ansi)
  const ansitext = TextToAnsi(text);
  WriteLog(ansitext);
  WriteLargeLog(side,ansitext);
}

function handleCONNECT(data) {
  let side = '';
  if(queue.qid === '-1'&&lsite.siteid !== '-1'&&rsite.siteid !== '-1') {
    doQUEUENEW();
  }// new queue
  if(data['SID'] === lsite.session) {
    side='left';
    const site = (side==='left')?lsite:rsite;
    localStorage.lastleftsite=lsite.siteid;
  } else if(data['SID'] === rsite.session) {
    side='right';
    const site = (side==='left')?lsite:rsite;
    localStorage.lastrightsite=rsite.siteid;
  } else {
    WriteLog('CONNECT msg received for unknown SID: '+data['SID']);
    return;
  }
  const site = (side==='left')?lsite:rsite;
  // if STARTDIR, call CWD. Which calls PWD. Otherwise, PWD.
  // when PWD returns, call DIRLIST.
  if('STARTDIR' in sitelist[site.siteid]&&sitelist[site.siteid]['STARTDIR'] !== '') {
    doCWD(site.session,sitelist[site.siteid]['STARTDIR']);
  } else {
    doPWD(site.session);
  }
}

function handleDISCONNECT(data) {
  let side = '';
  if(data['SID'] === lsite.session) {
    side='left';
  } else if(data['SID'] === rsite.session) {
    side='right'
  } else {
    if(!debug) {
      WriteLog('CONNECT msg received for unknown SID: '+data['SID']);
    }
    return;
  }
  const site = (side==='left')?lsite:rsite;

  // write out rdirs
  svCmdReadWrite('write','RDIRS',site.siteid);

  if(!debug) {
    WriteLog('Site ' + sitelist[site.siteid]['NAME'] + ' disconnected. ('+data['MSG']+')');
  }
  site.session = '-1';
  // reset siteid
  site.siteid = '-1';
  clearTable(side);
  if(queue.qid !== '-1') {
    doQUEUEFREE();
  }
}

function handleSESSIONFREE(data) {
  handleDISCONNECT(data);
}

function handleCWD(data) {
  // Call PWD to reflect new dir
  let side = '';
  if(data['SID'] === lsite.session) {
    side='left';
  } else if(data['SID'] === rsite.session) {
    side='right'
  } else {
    if(!debug) {
      WriteLog('CWD msg received for unknown SID: '+data['SID']);
    }
    return;
  }
  const site = (side==='left')?lsite:rsite;
  doPWD(site.session);
}

function handlePWD(data) {
  // PWD is allowed to fail, and we should use the textfield value.
  // if it succeeds, we can use the ftpd's path view.
  // Issue DIRLIST at end.
  let side = '';
  let site;
  let pd;
  if(data['SID'] === lsite.session) {
    side='left';
    site = lsite;
    pd = document.getElementById('lpath');
  } else if(data['SID'] === rsite.session) {
    side='right';
    site = rsite;
    pd = document.getElementById('rpath');
  } else {
    if(!debug) {
      WriteLog('PWD msg received for unknown SID: '+data['SID']);
    }
    return;
  }

  // Update the PATH input
  if(data['CODE'] === '0') {
    pd.value = decode(data['PATH']);
  }
  if(pd.value.indexOf('/') > -1) {
    rememberCWD(side,pd.value);
  }
  doDIRLIST(site.session);
}

function handleIDLE(data) {
}

function handleDIRLIST(data) {
  const sid = data['SID'];
  const side = (sid===lsite.session)?'left':'right';
  const sitedata = (sid===lsite.session)?lsite:rsite;
  if('BEGIN' in data) {
    sitedata.listing = [];
    sitedata.fidlisting = {};
    clearTable(side);
    dirlistStatus(side,'**PROCESSING**');
  } else if('END' in data) {
    refreshTable(side);
    if(!debug) {
      WriteLog('Directory listing complete.');
    }
  } else {
    if(sitedata.filter != null) {
      if(!data['NAME'].match(sitedata.filter)) {
        return;
      }
    }
    sitedata.listing.push(data);
    //WriteLog("sitedata listing for " + side + " pushed.");
    // used in refreshTable for access via fid
    for(let i = 0; i < sitedata.listing.length; i++) {
      sitedata.fidlisting[sitedata.listing[i]['FID']] = sitedata.listing[i];
    }
    //WriteLog("sitedata fidlisting for " + side + " pushed.");
  }
}

function handleQUEUENEW(data) { // QUEUENEW|CODE=0|QID=1|NORTH_SID=1|SOUTH_SID=2|MSG=Queue created
  queuenewlock = 0;
  if((data['NORTH_SID']===lsite.session) &&
        (data['SOUTH_SID']===rsite.session)) {
    queuesidesid = { NORTH: lsite.session, SOUTH: rsite.session };
    queue.qid = data['QID'];
  } else if((data['NORTH_SID']===rsite.session) &&
        (data['SOUTH_SID']===lsite.session)) {
    queuesidesid = { NORTH: rsite.session, SOUTH: lsite.session };
    queue.qid = data['QID'];
  } else {
    return;// not our queue
  }
  queue.listing = [];
  refreshTable('queue');
  if(!debug) {
    WriteLog('QueueID set to '+queue.qid);
  }
}

function handleQUEUEFREE(data) {
  if(data['QID'] === queue.qid) {
    queue.qid = '-1';
    queue.listing = [];
    refreshTable('queue');
    return;
  }
  WriteLog('Queue '+data['QID']+' released.');
}

function handleQADD(data) {
  if(data['QID'] !== queue.qid) {
    return;
  }

  // We cheat here, since we only ever "ADD" to the Queue, we can
  // assume to add new entries to the end of the table, ignoring
  // the value of "@" (which has the value of the position to insert).
  //var src = decode(data["SRCPATH"]);
  //var dst = decode(data["DSTPATH"]);
  // Use data["FID"] here to go get the "size" and "date" values.

  const qside = data['SRC'];
  const fid = data['FID'];
  const sid = queuesidesid[qside];
  const sitedata = (sid===lsite.session)?lsite:rsite;
  let size = '256000';
  let date = 'Jul 23 1985';
  if(sitedata.fidlisting[fid]['SIZE']) {
    size = sitedata.fidlisting[fid]['SIZE'];
  }
  if(sitedata.fidlisting[fid]['DATE']) {
    date = time2str(sitedata.fidlisting[fid]['DATE']);
  }
  data['SIZE'] = size;
  data['DATE'] = date;

  queue.listing.push(data);
  refreshTable('queue');

  //addTableRow('queue',"",
  //            ["<input type='checkbox' name='QITEM#"+data["@"]+"' value='QITEM#"+data["@"]+"'/>"+src,src],
  //            [0,""], // size
  //            [0,""], // date
  //            [dst,dst]);
}

function handleGO(data) {
  if(data['QID'] !== queue.qid) {
    return;
  }
  if(data['CODE'] !== '0') {
    return;
  }
  clearLog('output');
  WriteLog(data['MSG']);
  lsite.session = '-1';
  rsite.session = '-1';
  clearTable('left'); document.getElementById('lpath').value='';
  clearTable('right'); document.getElementById('rpath').value='';
}

function handleSTOP(data) {
  WriteLog(data['MSG']);
}
// QS|QID=2|XFREND|SRC=NORTH|TIME=0.11|KB/s=110.48
function handleQS(data) {
  const qid = data['QID'];

  if('START' in data) {
    if(qid === queue.qid) {
      WriteLog('Thinking about ' +data['SRCPATH']);
    }
    WriteQMLog('QID['+qid+'] Thinking about ' +data['SRCPATH']);
    expandstart = 0;
  }
  if('XFRACT' in data) {
    if(qid === queue.qid) {
      WriteLog('Transfer started from position '+data['REST']+' size '+data['SIZE']+' (Encrypted=' +data['SECURE']+')');
    }
    WriteQMLog('QID['+qid+'] Transfer started from position '+data['REST']+' size '+data['SIZE']+' (Encrypted=' +data['SECURE']+')');
  }

  if('XFREND' in data) {
    if(qid === queue.qid) {
      WriteLog('Transfer completed after '+data['TIME']+'s at '+data['KB/S']+'KB/s');
      const speedstats = document.getElementById('speedstats');
      speedstats.innerHTML = data['KB/S']+'KB/s';
    }
    WriteQMLog('QID['+qid+'] Transfer completed after '+data['TIME']+'s at '+data['KB/S']+'KB/s');
  }
  if('REFRESH' in data) {
    refreshTable('queue');
  }
}

// This function needs fixing. MoveRow is crappy, and we lose that it is checked.
function handleQMOVE(data) {
  move(queue.listing,data['FROM'],data['@']);
  refreshTable('queue');
}

function handleQDEL(data) {
  if(data['QID'] !== queue.qid) {
    return;
  }
  if(!debug) {
    WriteLog('Deleted queue item '+data['@']);
  }
  queue.listing.splice(data['@'],1);
  refreshTable('queue');
}

// QC|QID=2|REMOVE|@=0
function handleQC(data) {
  if(data['QID'] !== queue.qid) {
    return;
  }
  if('REMOVE' in data) {
    if(!debug) {
      WriteLog('Attempting to remove '+data['@']);
    }
    queue.listing.splice(data['@'],1);
  }
  if('EMPTY' in data) {
    queue.listing = [];
    if(!debug) {
      WriteLog('Queue processing completed.');
    }
  }
  if('MOVE' in data) {
    move(queue.listing,data['FROM'],data['TO']);
    if(!debug) {
      WriteLog('Moving queue item '+data['FROM']+' to '+data['TO']);
    }
  }
  if('INSERT' in data) {
    const date = time2str(data['DATE']);
    data['DATE'] = date;
    queue.listing.splice(data['@'],0,data);
    if(!debug) {
      WriteLog('Inserted new queue item '+data['SRCPATH']+' to '+data['DSTPATH']);
    }
    if('EXPANDING' in data) {
      if(expandstart === 0) {
        expandstart = 1;
      } else {
        return;
      }
    }
  }
  refreshTable('queue');
}

function handleSITE(data) {
  if(data['CODE'] !== '-1') {
    return;
  }
  handleLOG(data);
}

function handleQCOMPARE(data) {
  if(data['QID'] !== queue.qid) {
    return;
  }
  if('BEGIN' in data) {
    WriteLog('Comparing directories...');
    clearAllCheckboxes('left');
    clearAllCheckboxes('right');
    return;
  }
  if('END' in data) {
    WriteLog('Compare completed.');
    return;
  }
  if('NORTH_FID' in data) {
    setCheckboxes('left', data['NORTH_FID'].split(','));
  }
  if('SOUTH_FID' in data) {
    setCheckboxes('right', data['SOUTH_FID'].split(','));
  }
}

function handleMKD(data) {
  const sid = data['SID'];
  const side = (sid===lsite.session)?'left':'right';
  const sitedata = (sid===lsite.session)?lsite:rsite;
  if(data['CODE'] === '250') { // Success
    WriteLog('Directory created');
    return;
  }

  if('MSG' in data) {
    WriteLog('MKD ' + data['MSG']);
  }
}

function handleDELE(data) {
  const sid = data['SID'];
  const side = (sid===lsite.session)?'left':'right';
  const sitedata = (sid===lsite.session)?lsite:rsite;
  if(data['CODE'] === '0') { // Success
    WriteLog('File deleted.');
    return;
  }

  if('MSG' in data) {
    WriteLog('DELE ' + data['MSG']);
  }
}

function handleRMD(data) {
  const sid = data['SID'];
  const side = (sid===lsite.session)?'left':'right';
  const sitedata = (sid===lsite.session)?lsite:rsite;
  if(data['CODE'] === '250') { // Success
    WriteLog('Directory deleted.');
    return;
  }

  if('MSG' in data) {
    WriteLog('RMD ' + data['MSG']);
  }
}

function handleREN(data) {
  const sid = data['SID'];
  const side = (sid===lsite.session)?'left':'right';
  const sitedata = (sid===lsite.session)?lsite:rsite;
  if(data['CODE'] === '0') { // Success
    WriteLog('Rename completed.');
    return;
  }

  if('MSG' in data) {
    WriteLog('REN ' + data['MSG']);
  }
}

function handleQLIST(data) {
  if('BEGIN' in data) {
    queuemanager.listing = [];
    clearTable('qmqueue');
    return;
  }
  if('END' in data) {
    WriteLog('Queue list refreshed.');
    refreshTable('qmqueue');
    return;
  }
  //    WriteLog("Attempting to insert QLIST "+data["QID"]+" ("+data["NORTH"]+"<=>"+data["SOUTH"]+")");

  // Engine really should set this.
  if(data['QID'] === queue.qid) {
    data['SUBSCRIBED'] = 1;
  }

  queuemanager.listing.push(data);
}

function handleQCLONE(data) {
  WriteLog(data['MSG']);
}

function handleSITEADD(data) {
  if(data['CODE'] === '0') {
    WriteLog('Site added.');
    doSITELIST();
  }
}

function handleSITEMOD(data) {
  if(data['CODE'] === '0') {
    WriteLog('Site modified.');
    doSITELIST();
  }
}

function handleSITEDEL(data) {
  if(data['CODE'] === '0') {
    WriteLog('Site deleted.');
    doSITELIST();
  }
}

function handleSETPASS(data) {
  if(data['CODE'] === '0') {
    WriteLog('FXP.One password successfully changed.');
  } else {
    WriteLog('FXP.One password change failed: ' + data['MSG']);
  }
}

function handleSUBSCRIBE(data) {
  if('UNSUBSCRIBED' in data) {
    WriteQMLog('Unsubscribed from QID '+data['QID']);
  } else {
    WriteQMLog('Subscribed to QID '+data['QID']);
  }
}

function handleQGRAB(data) {
  if(data['CODE'] !== '0') {
    WriteQMLog('Unable to grab QID '+data['QID']+': '+data['MSG']);
    return;
  }

  queue.id = data['QID'];
  lsite.siteid = data['NORTH_SITEID'];
  rsite.siteid = data['SOUTH_SITEID'];

  lsite.session = '-1';
  if('NORTH_SID' in data) {
    lsite.session = data['NORTH_SID'];
  }

  rsite.session = '-1';
  if('SOUTH_SID' in data) {
    rsite.session = data['SOUTH_SID'];
  }

  // Update GUI to show site
  const pdl = document.getElementById('lslist');
  for(let i = 0; i < pdl.options.length; i++) {
    if(pdl.options[i].value === lsite.siteid) {
      pdl.selectedIndex = i;
      break;
    }
  }

  const pdr = document.getElementById('rslist');
  for(let i = 0; i < pdr.options.length; i++) {
    if(pdr.options[i].value === rsite.siteid) {
      pdr.selectedIndex = i;
      break;
    }
  }

  if('ITEMS' in data && data['ITEMS'] > 0) {
    doQGET(queue.id);
  }

  toggleView('primary');
  WriteLog('Grabbing queue '+data['NORTH_NAME']+' <=> '+data['SOUTH_NAME']);
}

function handleQGET(data) {
  if('BEGIN' in data) {
    queue.listing = [];
    refreshTable('queue');
    return;
  }

  if('END' in data) {
    WriteLog('Queue fetch complete.');
    refreshTable('queue');
    return;
  }

  queue.listing.push(data);
}

function handleQERR(data) {
  let str = '';

  if('BEGIN' in data) {
    WriteQMLog('Listing errors in QID '+data['QID']+' (Total '+data['ITEMS']+')');
    return;
  }
  if('END' in data) {
    WriteQMLog('Done.');
    return;
  }

  str = '['+data['QTYPE'] + '] '+data['SRCPATH']+' => '+data['DSTPATH'] + '   :';
  if('SERR_0' in data) {
    str+='SRCERR <'+data['SERR_0']+'> ';
  }
  if('DERR_0' in data) {
    str+='DSTERR <'+data['DERR_0']+'> ';
  }
  if('SERR_1' in data) {
    str+='SRCERR <'+data['SERR_1']+'> ';
  }
  if('DERR_1' in data) {
    str+='DSTERR <'+data['DERR_1']+'> ';
  }
  if('SERR_2' in data) {
    str+='SRCERR <'+data['SERR_2']+'> ';
  }
  if('DERR_2' in data) {
    str+='DSTERR <'+data['DERR_2']+'> ';
  }

  WriteQMLog(str);
}
