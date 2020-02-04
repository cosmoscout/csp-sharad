/* global IApi, CosmoScout */

/**
 * Sharad Api
 */
class SharadApi extends IApi {
  /**
   * @inheritDoc
   */
  name = 'sharad';

  /**
   * @param file {string}
   * @param time {string|number}
   */
  add(file, time) {
    const sharadList = document.getElementById('list-sharad');
    const sharad = CosmoScout.loadTemplateContent('sharad');

    sharad.innerHTML = sharad.innerHTML
      .replace(this.regex('FILE'), file)
      .replace(this.regex('TIME'), time)
      .trim();

    sharad.classList.add(`item-${file}`);

    sharadList.appendChild(sharad);

    CosmoScout.initDataCalls();
  }
}

(() => {
  CosmoScout.init(SharadApi);
})();
