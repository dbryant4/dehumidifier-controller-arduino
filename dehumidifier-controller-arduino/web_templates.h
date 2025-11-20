#ifndef WEB_TEMPLATES_H
#define WEB_TEMPLATES_H

// HTML template for firmware update page
const char* update_html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Dehumidifier Controller - Update Firmware</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; }
        .container { max-width: 600px; margin: 0 auto; }
        .progress { width: 100%; background-color: #f0f0f0; border-radius: 4px; }
        .progress-bar { height: 20px; background-color: #4CAF50; border-radius: 4px; width: 0%; }
        .status { margin-top: 20px; padding: 10px; border-radius: 4px; }
        .success { background-color: #dff0d8; color: #3c763d; }
        .error { background-color: #f2dede; color: #a94442; }
        .hidden { display: none; }
        button { padding: 10px 20px; background-color: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; }
        button:disabled { background-color: #cccccc; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Update Firmware</h1>
        <form id="uploadForm" action="/update" method="post" enctype="multipart/form-data">
            <input type="file" name="firmware" accept=".bin">
            <button type="submit" id="uploadButton">Upload Firmware</button>
        </form>
        <div id="progress" class="progress hidden">
            <div id="progressBar" class="progress-bar"></div>
        </div>
        <div id="status" class="status hidden"></div>
        <p><a href="/">Back to Main Page</a></p>
    </div>
    <script>
        document.getElementById('uploadForm').onsubmit = function(e) {
            e.preventDefault();
            var formData = new FormData(this);
            var xhr = new XMLHttpRequest();
            var progress = document.getElementById('progress');
            var progressBar = document.getElementById('progressBar');
            var status = document.getElementById('status');
            var uploadButton = document.getElementById('uploadButton');
            
            progress.classList.remove('hidden');
            status.classList.remove('hidden');
            uploadButton.disabled = true;
            
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    var percentComplete = (e.loaded / e.total) * 100;
                    progressBar.style.width = percentComplete + '%';
                }
            };
            
            xhr.onload = function() {
                if (xhr.status === 200) {
                    status.textContent = 'Update successful! Device will restart...';
                    status.className = 'status success';
                    setTimeout(function() {
                        window.location.href = '/';
                    }, 5000);
                } else {
                    status.textContent = 'Update failed: ' + xhr.responseText;
                    status.className = 'status error';
                    uploadButton.disabled = false;
                }
            };
            
            xhr.onerror = function() {
                status.textContent = 'Update failed: Network error';
                status.className = 'status error';
                uploadButton.disabled = false;
            };
            
            xhr.open('POST', '/update', true);
            xhr.send(formData);
        };
    </script>
</body>
</html>
)";

#endif // WEB_TEMPLATES_H

