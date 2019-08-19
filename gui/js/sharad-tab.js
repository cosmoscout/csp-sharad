function add_sharad(file, time) {
    const html = `
        <div class='row item-${file}''>
            <div class='col s10' >${file}</div>
            <div class='col s2'><a class='waves-effect waves-light btn glass block' onclick='window.call_native("set_time", ${time})' ><i class='material-icons'>restore</i></a></div>
        </div>`;

    $('#list-sharad').append(html);
}