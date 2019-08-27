function add_sharad(file, time) {
    const html = `
        <div class='row item-${file}''>
            <div class='col-8' >${file}</div>
            <div class='col-4'><a class='btn glass block' onclick='window.call_native("set_time", ${time})' ><i class='material-icons'>restore</i></a></div>
        </div>`;

    $('#list-sharad').append(html);
}