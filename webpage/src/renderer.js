const os = require('os');
const {ipcRenderer} = require('electron');
const { execFile } = require('child_process');

function plotBtnClick() {
    
    document.getElementById('plot_button').disabled = true;
    document.getElementById('plot_div').style.display = "block";

    var geneId = document.getElementById('gene_id').value;

    if(os.platform() === 'win32' || os.platform() === 'win64') {
        execFile('C:/Workspace/CSE549-txpCoverage/run.bat', [geneId], [windowsHide=false], (err, stdout, stderr) => {
            if(err) {
                console.error(err);
                return;
            }
            console.log(stdout);
        });
    } else {
        execFile('"../run.sh"', geneId, (err, stdout, stderr) => {
            if(err) {
                console.errror(err);
                return;
            }
            console.log(stdout);
        });
    }
//     var imgPath = '../assets/plots/' + 'ENST00000361567.2' + '.png';
//     var img = document.createElement('img');
//     img.setAttribute('src', imgPath);
//     img.setAttribute('class', 'plot');
//     document.getElementById('plot_div').appendChild(img);
};