// HTML-Template im Flash speichern (PROGMEM)
const char html_settings[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta http-equiv="content-type" content="text/html; charset=UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Aquacontrol Parameter Konfiguration</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #f0f0f0; }
    .container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
    .section { margin-bottom: 25px; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
    .section h3 { margin-top: 0; color: #2c3e50; border-bottom: 1px solid #eee; padding-bottom: 5px; }
    
    .param-table { width: 100%; border-collapse: collapse; margin: 10px 0; }
    .param-table td { padding: 8px 5px; vertical-align: top; border-bottom: 1px solid #f0f0f0; }
    .param-table tr:hover { background: #f9f9f9; }
    .param-label { font-weight: bold; text-align: right; padding-right: 15px; width: 40%; }
    .param-value { width: 60%; }
    
    input[type="text"], input[type="number"], input[type="password"] { width: 100%; padding: 6px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
    input[type="checkbox"] { margin-right: 5px; transform: scale(1.2); }
    .checkbox-group { margin: 5px 0; }
    
    .password-container { position: relative; display: flex; align-items: center; }
    .password-input { flex: 1; padding-right: 40px; }
    .toggle-password { 
      position: absolute; 
      right: 8px; 
      background: none; 
      border: none; 
      cursor: pointer; 
      font-size: 16px; 
      color: #666;
      padding: 4px;
      border-radius: 3px;
    }
    .toggle-password:hover { 
      background: #f0f0f0; 
      color: #333;
    }
    
    .submit-btn { background: #3498db; color: white; padding: 12px 30px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 15px; }
    .submit-btn:hover { background: #2980b9; }
    .info { font-size: 11px; color: #7f8c8d; margin-top: 2px; font-style: italic; }
    .status { padding: 10px; margin: 10px 0; border-radius: 4px; }
    .success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
    .loading { color: #856404; background: #fff3cd; border: 1px solid #ffeaa7; }
  </style>
  <script>
    function loadParameters() {
      fetch('/getParameters')
        .then(response => response.json())
        .then(data => {
          // Float Werte
          document.querySelector('[name="temp_alarmhigh_treshold"]').value = String(data.temp_alarmhigh_treshold).replace(',', '.')|| '';
          document.querySelector('[name="temp_alarmlow_treshold"]').value = String(data.temp_alarmlow_treshold).replace(',', '.') || '';
          
          // Intervalle
          document.querySelector('[name="Temp_Update_Interval_LIVE_mins"]').value = data.Temp_Update_Interval_LIVE_mins || '';
          document.querySelector('[name="Temp_Update_Interval_SIM_mins"]').value = data.Temp_Update_Interval_SIM_mins || '';
          document.querySelector('[name="backupInterval_mins"]').value = data.backupInterval_mins || '';
          document.querySelector('[name="maxcooling_mins"]').value = data.maxcooling_mins || '';
          
          // Checkboxen
          document.querySelector('[name="simulateSensor"]').checked = data.simulateSensor || false;
          document.querySelector('[name="emailme"]').checked = data.emailme || false;
          document.querySelector('[name="skipmail"]').checked = data.skipmail || false;
          document.querySelector('[name="serialout"]').checked = data.serialout || false;
          document.querySelector('[name="measure"]').checked = data.measure || false;
          
          // Technische Einstellungen
          document.querySelector('[name="pwmfrequency"]').value = data.pwmfrequency || '';
          document.querySelector('[name="weeklyReport_tm_wday"]').value = data.weeklyReport_tm_wday ?? ''; // Nullish Coalescing Operator (??)
          document.querySelector('[name="weeklyReport_tm_hour"]').value = data.weeklyReport_tm_hour ?? '';
          document.querySelector('[name="weeklyReport_tm_min"]').value = data.weeklyReport_tm_min ?? '';
          
          // Netzwerk
          document.querySelector('[name="ssid"]').value = data.ssid || '';
          document.querySelector('[name="password"]').value = data.password || '';
          document.querySelector('[name="ntpServer"]').value = data.ntpServer || '';
          
          // E-Mail
          document.querySelector('[name="smtp_AUTH_EMAIL"]').value = data.smtp_AUTH_EMAIL || '';
          document.querySelector('[name="smtp_AUTH_PASSWORD"]').value = data.smtp_AUTH_PASSWORD || '';
          document.querySelector('[name="smtp_RECIPIENT_EMAIL"]').value = data.smtp_RECIPIENT_EMAIL || '';
          
          document.getElementById('loadingStatus').style.display = 'none';
        })
        .catch(error => {
          console.error('Fehler beim Laden der Parameter:', error);
          document.getElementById('loadingStatus').innerHTML = 'Fehler beim Laden der Parameter';
        });
    }

    function togglePasswordVisibility(inputId, button) {
      const input = document.getElementById(inputId);
      if (input.type === 'password') {
        input.type = 'text';
        button.innerHTML = '🙈'; // Auge zu Icon
        button.title = 'Passwort verbergen';
      } else {
        input.type = 'password';
        button.innerHTML = '👁️'; // Auge auf Icon
        button.title = 'Passwort anzeigen';
      }
    }

    function initializePasswordToggles() {
      // WiFi Passwort Toggle
      const wifiPasswordInput = document.querySelector('[name="password"]');
      if (wifiPasswordInput) {
        const wifiContainer = document.createElement('div');
        wifiContainer.className = 'password-container';
        wifiPasswordInput.parentNode.insertBefore(wifiContainer, wifiPasswordInput);
        wifiContainer.appendChild(wifiPasswordInput);
        wifiPasswordInput.className = 'password-input';
        wifiPasswordInput.type = 'password';
        wifiPasswordInput.id = 'wifiPassword';
        
        const wifiToggle = document.createElement('button');
        wifiToggle.type = 'button';
        wifiToggle.className = 'toggle-password';
        wifiToggle.innerHTML = '👁️';
        wifiToggle.title = 'Passwort anzeigen';
        wifiToggle.onclick = () => togglePasswordVisibility('wifiPassword', wifiToggle);
        wifiContainer.appendChild(wifiToggle);
      }

      // SMTP Passwort Toggle
      const smtpPasswordInput = document.querySelector('[name="smtp_AUTH_PASSWORD"]');
      if (smtpPasswordInput) {
        const smtpContainer = document.createElement('div');
        smtpContainer.className = 'password-container';
        smtpPasswordInput.parentNode.insertBefore(smtpContainer, smtpPasswordInput);
        smtpContainer.appendChild(smtpPasswordInput);
        smtpPasswordInput.className = 'password-input';
        smtpPasswordInput.type = 'password';
        smtpPasswordInput.id = 'smtpPassword';
        
        const smtpToggle = document.createElement('button');
        smtpToggle.type = 'button';
        smtpToggle.className = 'toggle-password';
        smtpToggle.innerHTML = '👁️';
        smtpToggle.title = 'Passwort anzeigen';
        smtpToggle.onclick = () => togglePasswordVisibility('smtpPassword', smtpToggle);
        smtpContainer.appendChild(smtpToggle);
      }
    }
    
    window.onload = function() {
      loadParameters();
      // Kurz verzögert initialisieren, damit die Input-Felder vorhanden sind
      setTimeout(initializePasswordToggles, 100);
    };
  </script>
</head>
<body>
  <div class="container">
    <h1>Aquacontrol V4 Parameter Konfiguration</h1>
    
    <div id="loadingStatus" class="status loading">
      Lade Parameter...
    </div>
    
    <form action="/saveParams" method="POST" onsubmit="return validateForm()">
      
      <div class="section">
        <h3>Temperatur Einstellungen</h3>
        <table class="param-table">
          <tr>
            <td class="param-label">Temperaturalarm oberhalb °C:</td>
            <td class="param-value">
              <input type="number" step="0.1" name="temp_alarmhigh_treshold" placeholder="28.5">
            </td>
          </tr>
          <tr>
            <td class="param-label">Temperaturalarm unterhalb °C:</td>
            <td class="param-value">
              <input type="number" step="0.1" name="temp_alarmlow_treshold" placeholder="19.5">
            </td>
          </tr>
        </table>
      </div>

      <div class="section">
        <h3>Update-Intervalle</h3>
        <table class="param-table">
          <tr>
            <td class="param-label">Live-Update Intervall (min):</td>
            <td class="param-value">
              <input type="number" name="Temp_Update_Interval_LIVE_mins" placeholder="20">
              <div class="info">z.B. 20 Minuten</div>
            </td>
          </tr>
          <tr>
            <td class="param-label">Simulations-Update Intervall (min):</td>
            <td class="param-value">
              <input type="number" name="Temp_Update_Interval_SIM_mins" placeholder="1">
              <div class="info">z.B. 1 Minute</div>
            </td>
          </tr>
          <tr>
            <td class="param-label">Backup Intervall (min):</td>
            <td class="param-value">
              <input type="number" name="backupInterval_mins" placeholder="240">
              <div class="info">z.B. 240 Minuten = 4 Stunden</div>
            </td>
          </tr>
          <tr>
            <td class="param-label">Maximale Kühlzeit (min):</td>
            <td class="param-value">
              <input type="number" name="maxcooling_mins" placeholder="180">
              <div class="info">z.B. 120 Minuten = 2 Stunden</div>
            </td>
          </tr>
        </table>
      </div>

      <div class="section">
        <h3>Betriebsmodi</h3>
        <table class="param-table">
          <tr>
            <td class="param-label">Temperatursensor simulieren:</td>
            <td class="param-value">
              <div class="checkbox-group">
                <input type="checkbox" name="simulateSensor" id="simulateSensor">
                <label for="simulateSensor">Aktivieren</label>
              </div>
            </td>
          </tr>
          <tr>
            <td class="param-label">E-Mails verschicken:</td>
            <td class="param-value">
              <div class="checkbox-group">
                <input type="checkbox" name="emailme" id="emailme">
                <label for="emailme">Aktivieren</label>
              </div>
            </td>
          </tr>
          <tr>
            <td class="param-label">E-Mails nur simulieren:</td>
            <td class="param-value">
              <div class="checkbox-group">
                <input type="checkbox" name="skipmail" id="skipmail">
                <label for="skipmail">Aktivieren (nicht wirklich senden)</label>
              </div>
            </td>
          </tr>
          <tr>
            <td class="param-label">Ausgaben über Serial Port:</td>
            <td class="param-value">
              <div class="checkbox-group">
                <input type="checkbox" name="serialout" id="serialout">
                <label for="serialout">Aktivieren</label>
              </div>
            </td>
          </tr>
          <tr>
            <td class="param-label">Temperaturen erfassen:</td>
            <td class="param-value">
              <div class="checkbox-group">
                <input type="checkbox" name="measure" id="measure">
                <label for="measure">Aktivieren</label>
              </div>
            </td>
          </tr>
        </table>
      </div>

      <div class="section">
        <h3>Technische Einstellungen</h3>
        <table class="param-table">
          <tr>
            <td class="param-label">PWM Frequenz:</td>
            <td class="param-value">
              <input type="number" name="pwmfrequency" placeholder="4000">
            </td>
          </tr>
          <tr>
            <td class="param-label">Wöchentlicher Report - Wochentag:</td>
            <td class="param-value">
              <input type="number" min="0" max="6" name="weeklyReport_tm_wday" placeholder="5">
              <div class="info">0-6 (0=Sonntag)</div>
            </td>
          </tr>
          <tr>
            <td class="param-label">Wöchentlicher Report - Stunde:</td>
            <td class="param-value">
              <input type="number" min="0" max="23" name="weeklyReport_tm_hour" placeholder="22">
              <div class="info">0-23</div>
            </td>
          </tr>
          <tr>
            <td class="param-label">Wöchentlicher Report - Minute:</td>
            <td class="param-value">
              <input type="number" min="0" max="59" name="weeklyReport_tm_min" placeholder="0">
              <div class="info">0-59</div>
            </td>
          </tr>
        </table>
      </div>

      <div class="section">
        <h3>Netzwerk Einstellungen</h3>
        <table class="param-table">
          <tr>
            <td class="param-label">WiFi SSID:</td>
            <td class="param-value">
              <input type="text" name="ssid" placeholder="fritzzzz">
            </td>
          </tr>
          <tr>
            <td class="param-label">WiFi Passwort:</td>
            <td class="param-value">
              <!-- Container wird per JavaScript hinzugefügt -->
              <input type="password" name="password" placeholder="51...0">
            </td>
          </tr>
          <tr>
            <td class="param-label">NTP Server:</td>
            <td class="param-value">
              <input type="text" name="ntpServer" placeholder="fritz.box">
            </td>
          </tr>
        </table>
      </div>

      <div class="section">
        <h3>E-Mail Einstellungen</h3>
        <table class="param-table">
          <tr>
            <td class="param-label">SMTP Authentifizierungs-E-Mail:</td>
            <td class="param-value">
              <input type="text" name="smtp_AUTH_EMAIL" placeholder="sc..2@gmail.com">
            </td>
          </tr>
          <tr>
            <td class="param-label">SMTP Passwort (App-Passwort):</td>
            <td class="param-value">
              <!-- Container wird per JavaScript hinzugefügt -->
              <input type="password" name="smtp_AUTH_PASSWORD" placeholder="gcu...cfu">
            </td>
          </tr>
          <tr>
            <td class="param-label">Empfänger E-Mail:</td>
            <td class="param-value">
              <input type="text" name="smtp_RECIPIENT_EMAIL" placeholder="s...8@gmx.de">
            </td>
          </tr>
        </table>
      </div>

      <input type="submit" value="Parameter speichern" class="submit-btn">
    </form>
  </div>
  <nav class="tab-navigation">
        <ul class="tab-container">
            <li class="tab-item">
                <a href="/schedule" class="tab-link">Zeitschaltuhr</a>
            </li>
            <li class="tab-item">
                <a href="/light" class="tab-link">Licht Direkt schalten</a>
            </li>
            <li class="tab-item">
                <a href="/email" class="tab-link">Email abschicken</a>
            </li>
            <li class="tab-item">
                <a href="/info" class="tab-link">Temperaturverlauf</a>
            </li>
            <li class="tab-item">
                <a href="/csv" class="tab-link">CSV download</a>
            </li>
            <li class="tab-item">
                <a href="/settings" class="tab-link">Einstellungen</a>
            </li>
            <li class="tab-item">
                <a href="/restart" class="tab-link">Neustart</a>
            </li>
            <li class="tab-item">
                <a href="/ram" class="tab-link">RAM-Info</a>
            </li>
        </ul>
    </nav>
  
  <script>
    function validateForm() {
      // Einfache Validierung - kann erweitert werden
      const ssid = document.querySelector('[name="ssid"]').value;
      if (!ssid) {
        alert('Bitte WiFi SSID eingeben');
        return false;
      }
      return true;
    }
  </script>
</body>
</html>
)rawliteral";