// HTML-Template im Flash speichern (PROGMEM)
const char html_schedule[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <link rel="shortcut icon" href="#">
    <title>LED Zeitschaltung mit Fading</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 20px auto;
            padding: 20px;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }
        th, td {
            padding: 12px;
            border: 1px solid #ddd;
            text-align: left;
        }
        th {
            background-color: #f5f5f5;
        }
        .time-picker {
            display: flex;
            gap: 5px;
            align-items: center;
        }
        select {
            padding: 6px;
            border-radius: 4px;
            border: 1px solid #ccc;
        }
        .brightness-select {
            width: 120px;
        }
        .fade-select,
		.device-select {
            width: 100px;
        }
        .add-button {
            margin-top: 10px;
            padding: 8px 16px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .remove-btn {
            background-color: #ff4444;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            cursor: pointer;
        }
    </style>
</head>
<body>
    <h2>LED Zeitschaltung mit Fading V4</h2>
    
    <table id="scheduleTable">
        <thead>
            <tr>
                <th>Uhrzeit</th>
                <th>Helligkeit</th>
                <th>Dimmzeit (min)</th>
				<th>Ausgang</th>
                <th>Aktion</th>
            </tr>
        </thead>
        <tbody id="scheduleBody">
            <!-- Zeilen werden hier eingefügt -->
        </tbody>
    </table>

    <button class="add-button" onclick="addRow()">+ Neue Zeit hinzufügen</button>
    <button class="add-button" onclick="saveSchedule()" style="background-color: #2196F3;">Speichern</button>
	<button class="add-button" onclick="drawScheduleGraph()" style="background-color: #FF9800;">Aktualisieren</button>
	
	<div id="graphContainer" style="margin-top: 30px;">
        <h3>Beleuchtungszeiten</h3>
        <svg id="scheduleGraph" width="800" height="200" style="border: 1px solid #ccc;"></svg>
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
            <!-- <li class="tab-item">
                <a href="/restart" class="tab-link">Neustart</a>
            </li> -->
            <li class="tab-item">
                <a href="/ram" class="tab-link">RAM-Info</a>
            </li>
        </ul>
    </nav>
    <script>
        let rowCount = 0;
        const maxRows = 20;

        document.addEventListener('DOMContentLoaded', function() {
            fetch('/getSchedule')
                .then(response => response.json())
                .then(data => {
                data.forEach(entry => {
                    addRowWithData({
                    hour: entry.hour,
                    minute: entry.minute,
                    brightness: entry.brightness,
                    fadeDuration: entry.fadeDuration,
                    device: entry.device
                    });
                });
				drawScheduleGraph(); 
                })
                .catch(error => console.error('Fehler beim Laden:', error));
            });
		
            function drawScheduleGraph() {
                const svg = document.getElementById('scheduleGraph');
                svg.innerHTML = '';
                
                const allEntries = getCurrentEntries()
                    .filter(entry => entry.hour !== 99 && entry.minute !== 99)
                    .sort(sortComparator);
                
                // Trenne Einträge nach Device
                const entriesDevice0 = allEntries.filter(entry => entry.device === 0);
                const entriesDevice3 = allEntries.filter(entry => entry.device === 2);
                
                if (allEntries.length === 0) return;

                const width = 800;
                const height = 200;
                const margin = { top: 20, right: 30, bottom: 40, left: 100 };
                const graphWidth = width - margin.left - margin.right;
                const graphHeight = height - margin.top - margin.bottom;

                // Hilfsfunktionen für Koordinaten
                const timeToX = (hour, minute) => {
                    const totalMinutes = hour * 60 + minute;
                    return margin.left + (totalMinutes / 1440) * graphWidth;
                };

                const brightnessToY = (brightness) => {
                    const yValues = {0: 180, 1: 165, 2: 150, 3: 130, 4: 90, 5: 30, 6: 0};
                    return margin.top + (yValues[brightness] || 0) * (graphHeight / 180);
                };

                // Funktion zum Zeichnen von Linien
                const drawLine = (x1, y1, x2, y2, color = 'blue', width = 2) => {
                    const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
                    line.setAttribute('x1', x1);
                    line.setAttribute('y1', y1);
                    line.setAttribute('x2', x2);
                    line.setAttribute('y2', y2);
                    line.setAttribute('stroke', color);
                    line.setAttribute('stroke-width', width);
                    svg.appendChild(line);
                };

                // Funktion zum Zeichnen von Kreisen
                const drawCircle = (cx, cy, r = 5, color = 'red') => {
                    const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
                    circle.setAttribute('cx', cx);
                    circle.setAttribute('cy', cy);
                    circle.setAttribute('r', r);
                    circle.setAttribute('fill', color);
                    circle.setAttribute('class', 'data-point');
                    svg.appendChild(circle);
                };

                // Funktion zum Zeichnen von Polygonen (für Flächen)
                const drawPolygon = (points, color = 'rgba(0,128,0,0.3)', stroke = 'none') => {
                    const polygon = document.createElementNS('http://www.w3.org/2000/svg', 'polygon');
                    polygon.setAttribute('points', points);
                    polygon.setAttribute('fill', color);
                    polygon.setAttribute('stroke', stroke);
                    svg.appendChild(polygon);
                };

                // Funktion zum Hinzufügen von Text
                const drawText = (x, y, text, anchor = 'start', fontSize = 12) => {
                    const textEl = document.createElementNS('http://www.w3.org/2000/svg', 'text');
                    textEl.setAttribute('x', x);
                    textEl.setAttribute('y', y);
                    textEl.setAttribute('text-anchor', anchor);
                    textEl.setAttribute('font-size', fontSize);
                    textEl.setAttribute('class', 'axis-label');
                    textEl.textContent = text;
                    svg.appendChild(textEl);
                };

                // Zeichne Achsen
                drawLine(margin.left, margin.top, margin.left, margin.top + graphHeight);
                drawLine(margin.left, margin.top + graphHeight, margin.left + graphWidth, margin.top + graphHeight);

                // Zeichne Y-Achsen Beschriftung (Helligkeitsstufen)
                const brightnessLabels = {
                    0: 'Stufe 0 (0%)',
                    1: 'Stufe 1 (5%)',
                    2: 'Stufe 2 (10%)',
                    3: 'Stufe 3 (25%)',
                    4: 'Stufe 4 (50%)',
                    5: 'Stufe 5 (84%)',
                    6: 'Stufe 6 (100%)'
                };
                
                Object.entries(brightnessLabels).forEach(([level, label]) => {
                    const y = brightnessToY(parseInt(level));
                    drawText(margin.left - 10, y + 4, label, 'end');
                    drawLine(margin.left - 5, y, margin.left + 5, y, '#ccc', 1);
                });

                // Zeichne X-Achsen Beschriftung (Zeit)
                for (let hour = 0; hour <= 24; hour += 3) {
                    const x = timeToX(hour, 0);
                    drawText(x, height - 10, `${hour.toString().padStart(2, '0')}:00`, 'middle');
                    drawLine(x, height - margin.bottom, x, height - margin.bottom + 5, '#ccc', 1);
                }

                // Zeichne Verlauf für Device 0 (blaue Linie) - ZUERST zeichnen
                let lastEndTime0 = 0;
                let lastBrightness0 = 0; // Es wird angenommen, daß das Licht abgeschaltet war.
                
                entriesDevice0.forEach((entry, index) => {
                    const startTime = entry.hour * 60 + entry.minute;
                    const endTime = startTime + parseInt(entry.fadeDuration);
                    const targetBrightness = entry.brightness;
                    
                    // Startpunkt (Beginn der Änderung)
                    const startX = timeToX(entry.hour, entry.minute);
                    const startY = brightnessToY(lastBrightness0);
                    
                    // Endpunkt (Ende der Dimmzeit)
                    const endX = timeToX(Math.floor(endTime / 60), endTime % 60);
                    const endY = brightnessToY(targetBrightness);
                    
                    // Zeichne horizontale Linie vom letzten Endpunkt bis zum aktuellen Startpunkt
                    if (index > 0) {
                        const lastEndX = timeToX(Math.floor(lastEndTime0 / 60), lastEndTime0 % 60);
                        drawLine(lastEndX, lastEndY0, startX, startY, 'blue', 2);
                    }
                    
                    // Zeichne Dimmverlauf (schräge Linie)
                    drawLine(startX, startY, endX, endY, 'blue', 2);
                    
                    // Zeichne Punkte
                    drawCircle(startX, startY, 5, '#2196F3'); // Blauer Punkt: Start der Änderung
                    drawCircle(endX, endY, 5, '#4CAF50');    // Grüner Punkt: Ende der Dimmzeit
                    
                    // Aktualisiere letzte Werte für nächste Iteration
                    lastEndTime0 = endTime;
                    lastBrightness0 = targetBrightness;
                    lastEndY0 = endY;
                });

                // Zeichne Verlauf für Device 3 (grüne Linie mit Fläche) - DANACH zeichnen
                let lastEndTime3 = 0;
                let lastBrightness3 = 0;
                let polygonPoints = '';
                
                // Sammle alle Punkte für das Polygon (Fläche unter der Kurve)
                entriesDevice3.forEach((entry, index) => {
                    const startTime = entry.hour * 60 + entry.minute;
                    const endTime = startTime + parseInt(entry.fadeDuration);
                    const targetBrightness = entry.brightness;
                    
                    // Startpunkt (Beginn der Änderung)
                    const startX = timeToX(entry.hour, entry.minute);
                    const startY = brightnessToY(lastBrightness3);
                    
                    // Endpunkt (Ende der Dimmzeit)
                    const endX = timeToX(Math.floor(endTime / 60), endTime % 60);
                    const endY = brightnessToY(targetBrightness);
                    
                    // Füge Punkte für die Fläche hinzu
                    if (index === 0) {
                        polygonPoints += `${startX},${height - margin.bottom} `; // Unterer linker Startpunkt
                    }
                    polygonPoints += `${startX},${startY} `; // Startpunkt der Linie
                    polygonPoints += `${endX},${endY} `;     // Endpunkt der Linie
                    
                    // Letzter Punkt: füge unteren rechten Endpunkt hinzu
                    if (index === entriesDevice3.length - 1) {
                        polygonPoints += `${endX},${height - margin.bottom} `; // Unterer rechter Endpunkt
                    }
                    
                    // Zeichne horizontale Linie vom letzten Endpunkt bis zum aktuellen Startpunkt
                    if (index > 0) {
                        const lastEndX = timeToX(Math.floor(lastEndTime3 / 60), lastEndTime3 % 60);
                        drawLine(lastEndX, lastEndY3, startX, startY, 'green', 2);
                    }
                    
                    // Zeichne Dimmverlauf (schräge Linie)
                    drawLine(startX, startY, endX, endY, 'green', 2);
                    
                    // Zeichne Punkte
                    drawCircle(startX, startY, 5, '#4CAF50'); // Grüner Punkt: Start der Änderung
                    drawCircle(endX, endY, 5, '#2E7D32');    // Dunkelgrüner Punkt: Ende der Dimmzeit
                    
                    // Aktualisiere letzte Werte für nächste Iteration
                    lastEndTime3 = endTime;
                    lastBrightness3 = targetBrightness;
                    lastEndY3 = endY;
                });
                
                // Zeichne die transparente Fläche unter der Device 3 Kurve
                if (polygonPoints) {
                    drawPolygon(polygonPoints, 'rgba(0,128,0,0.2)');
                }
                
                // Zeichne Titel
                drawText(width / 2, 15, 'Helligkeitsverlauf über 24 Stunden', 'middle', 14);
            }


        // Funktion zum Hinzufügen einer Zeile mit Daten
        function addRowWithData(entry) {
            if (rowCount >= maxRows) return;

            const entries = getCurrentEntries();
            const [hour, minute] = entry.time.split(':');
            
            entries.push({
                hour: parseInt(hour),
                minute: parseInt(minute),
                brightness: entry.brightness,
                fadeDuration: entry.fadeDuration,
                device: 0
            });
            
            renderSortedEntries(entries.sort(sortComparator));
        }


        function createTimeSelect(hourVal = '', minuteVal = '') {
            const container = document.createElement('div');
            container.className = 'time-picker';
            
            const hourSelect = document.createElement('select');
            hourSelect.className = 'hour';
            hourSelect.innerHTML = '<option value="">-- Stunde --</option>' + 
                Array.from({length: 24}, (_, i) => 
                    `<option value="${i}" ${i == hourVal ? 'selected' : ''}>${i.toString().padStart(2, '0')}</option>`
                ).join('');
            
            const minuteSelect = document.createElement('select');
            minuteSelect.className = 'minute';
            minuteSelect.innerHTML = '<option value="">-- Minute --</option>' + 
                Array.from({length: 60}, (_, i) => 
                    `<option value="${i}" ${i == minuteVal ? 'selected' : ''}>${i.toString().padStart(2, '0')}</option>`
                ).join('');

            container.appendChild(hourSelect);
            container.appendChild(document.createTextNode(':'));
            container.appendChild(minuteSelect);
            
            return container;
        }

        function createBrightnessSelect(brightnessVal = 1) {
            const select = document.createElement('select');
            select.className = 'brightness';
            select.innerHTML = `
                <option value="0" ${brightnessVal == 0 ? 'selected' : ''}>Stufe 0 (0%)</option>
                <option value="1" ${brightnessVal == 1 ? 'selected' : ''}>Stufe 1 (5%)</option>
                <option value="2" ${brightnessVal == 2 ? 'selected' : ''}>Stufe 2 (10%)</option>
                <option value="3" ${brightnessVal == 3 ? 'selected' : ''}>Stufe 3 (25%)</option>
                <option value="4" ${brightnessVal == 4 ? 'selected' : ''}>Stufe 4 (50%)</option>
                <option value="5" ${brightnessVal == 5 ? 'selected' : ''}>Stufe 5 (84%)</option>
                <option value="6" ${brightnessVal == 6 ? 'selected' : ''}>Stufe 6 (100%)</option>
            `;
            return select;
        }

        function createFadeSelect(fadeDuration = 0) {
            const select = document.createElement('select');
            select.className = 'fade-duration';
            select.innerHTML = Array.from({length: 61}, (_, i) => 
                `<option value="${i}" ${i == fadeDuration ? 'selected' : ''}>${i} min</option>`
            ).join('');
            return select;
        }
		
		function createDeviceSelect(deviceVal = 0) {
			const select = document.createElement('select');
			select.className = 'device';
			select.innerHTML = `
				<option value="0" ${deviceVal == 0 ? 'selected' : ''}>PWMLIGHT</option>
				<option value="1" ${deviceVal == 1 ? 'selected' : ''}>COOLING</option>
				<option value="2" ${deviceVal == 2 ? 'selected' : ''}>LIGHT (on/off)</option>
				<option value="3" ${deviceVal == 3 ? 'selected' : ''}>CO2 (on/off)</option>
				<option value="4" ${deviceVal == 4 ? 'selected' : ''}>MOON (on/off)</option>
			`;
			return select;
		}


        function getCurrentEntries() {
            const entries = [];
            document.querySelectorAll('#scheduleBody tr').forEach(row => {
                const hour = row.querySelector('.hour').value;
                const minute = row.querySelector('.minute').value;
                const brightness = row.querySelector('.brightness').value;
                const fadeDuration = row.querySelector('.fade-duration').value;
                const device = row.querySelector('.device').value; 
                
                entries.push({
                    hour: hour ? parseInt(hour) : 99,
                    minute: minute ? parseInt(minute) : 99,
                    brightness: brightness ? parseInt(brightness) : 1,
                    fadeDuration: parseInt(fadeDuration),
                    device: parseInt(device)
                });
            });
            return entries;
        }

        // Vergleichsfunktion für die Sortierung
        function sortComparator(a, b) {
            if (a.hour === b.hour) {
                return a.minute - b.minute;
            }
            return a.hour - b.hour;
        }

        // Angepasste renderSortedEntries-Funktion
        function renderSortedEntries(entries) {
            const tbody = document.getElementById('scheduleBody');
            tbody.innerHTML = '';
            rowCount = 0;

            entries.sort(sortComparator).forEach(entry => {
                const row = document.createElement('tr');
                
                // Zeitauswahl
                const timeCell = document.createElement('td');
                timeCell.appendChild(createTimeSelect(entry.hour, entry.minute));
                
                // Helligkeit
                const brightnessCell = document.createElement('td');
                brightnessCell.appendChild(createBrightnessSelect(entry.brightness));
                
                // Dimmzeit
                const fadeCell = document.createElement('td');
                fadeCell.appendChild(createFadeSelect(entry.fadeDuration));
				
				// Device-Auswahl
				const deviceCell = document.createElement('td');
				deviceCell.appendChild(createDeviceSelect(entry.device));
                
                // Lösch-Button
                const actionCell = document.createElement('td');
                const removeBtn = document.createElement('button');
                removeBtn.className = 'remove-btn';
                removeBtn.textContent = 'Entfernen';
                removeBtn.onclick = () => {
                row.remove();
                rowCount--;
                sortAndRenderRows();
                };
                
                actionCell.appendChild(removeBtn);
                row.appendChild(timeCell);
                row.appendChild(brightnessCell);
                row.appendChild(fadeCell);
				row.appendChild(deviceCell);
                row.appendChild(actionCell);
                tbody.appendChild(row);
                rowCount++;
            });
        }

        function sortAndRenderRows() {
            const entries = getCurrentEntries();
            const sorted = entries.sort((a, b) => {
                if (a.hour === b.hour) return a.minute - b.minute;
                return a.hour - b.hour;
            });
            renderSortedEntries(sorted);
        }

        function addRowWithData(entry) {
            if (rowCount >= maxRows) {
                alert("Maximal 20 Zeilen erlaubt!");
                return;
            }

            const entries = getCurrentEntries();
            entries.push({
                hour: entry.hour,
                minute: entry.minute,
                brightness: entry.brightness,
                fadeDuration: entry.fadeDuration,
                device: entry.device
            });
            
            renderSortedEntries(entries);
        }

        function addRow() {
            if (rowCount >= maxRows) {
                alert("Maximal 20 Zeilen erlaubt!");
                return;
            }
            
            const entries = getCurrentEntries();
            entries.push({
                hour: 99,
                minute: 99,
                brightness: 1,
                fadeDuration: 0,
                device: 0  // Device auf 0 setzen
            });
            renderSortedEntries(entries.sort((a, b) => {
                if (a.hour === b.hour) return a.minute - b.minute;
                return a.hour - b.hour;
            }))
            drawScheduleGraph(); 
        }

        function saveSchedule() {
            const schedule = getCurrentEntries()
                .filter(entry => entry.hour !== 99 && entry.minute !== 99)
                .map(entry => ({
                hour: entry.hour,
                minute: entry.minute,
                brightness: entry.brightness,
                fadeDuration: entry.fadeDuration,
                device: entry.device  // Device-Wert speichern
                }));

            fetch('/setSchedule', {
                method: 'POST',
                headers: {
                'Content-Type': 'application/json',
                },
                body: JSON.stringify(schedule)
            })
            .then(response => response.json())
            .then(data => {
                alert('Zeitplan erfolgreich gespeichert!');
            })
            .catch(error => {
                alert('Fehler beim Speichern: ' + error);
            });
            drawScheduleGraph(); 
        }
    </script>
</body>
</html>
)rawliteral";