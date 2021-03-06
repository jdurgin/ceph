import { browser } from 'protractor';
import { BucketsPageHelper } from './rgw/buckets.po';

export class Helper {
  static EC = browser.ExpectedConditions;
  static TIMEOUT = 30000;

  buckets: BucketsPageHelper;

  constructor() {
    this.buckets = new BucketsPageHelper();
  }

  /**
   * Checks if there are any errors on the browser
   *
   * @static
   * @memberof Helper
   */
  static checkConsole() {
    browser
      .manage()
      .logs()
      .get('browser')
      .then(function(browserLog) {
        browserLog = browserLog.filter((log) => {
          return log.level.value > 900; // SEVERE level
        });

        if (browserLog.length > 0) {
          console.log('\n log: ' + require('util').inspect(browserLog));
        }

        expect(browserLog.length).toEqual(0);
      });
  }
}
