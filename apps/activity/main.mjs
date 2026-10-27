import {createGateway} from './gateway.mjs';
import {DiscordOAuth} from './oauth.mjs';
import {requireThat} from './policy.mjs';
// No command-line secrets, public bind, test OAuth or arbitrary upstream options.
try {
  const clientId=process.env.PFME_DISCORD_CLIENT_ID,clientSecret=process.env.PFME_DISCORD_CLIENT_SECRET;
  requireThat(/^[0-9]{17,20}$/.test(clientId??'')&&typeof clientSecret==='string'&&clientSecret.length>=16,'Configuration');
  const origin=`https://${clientId}.discordsays.com`,invited=(process.env.PFME_DISCORD_INVITED_IDS??'').split(',');
  requireThat(invited.length>0&&invited.length<=8&&invited.every(id=>/^[0-9]{17,20}$/.test(id)),'Configuration');
  const port=Number(process.env.PFME_ACTIVITY_PORT??18766),backendPort=Number(process.env.PFME_BACKEND_PORT??18765),backendSecret=process.env.PFME_DEV_SECRET;
  requireThat(Number.isInteger(port)&&port>1024&&port<65536&&port!==backendPort,'Configuration');
  const gateway=await createGateway({origin,clientId,invited,backendPort,backendSecret},{oauth:new DiscordOAuth(clientId,clientSecret)});
  await gateway.listen(port);console.log(JSON.stringify({event:'loopback_ready',port}));
  let stopping=false;const stop=async()=>{if(stopping)return;stopping=true;await gateway.close();};
  process.on('SIGINT',stop);process.on('SIGTERM',stop);if(process.platform==='win32')process.on('SIGBREAK',stop);
}catch{console.error(JSON.stringify({event:'startup_failed'}));process.exitCode=1;}
