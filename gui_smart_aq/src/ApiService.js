export class ApiService {
    
    static apiAddress = "http://first-co2-bottle-scale.fritz.box"

    static async GetLoad() {
        const result = fetch(this.apiAddress + "/api/v1/load", { mode : 'cors'}).then(body => body.json());
        return result;
    }

    static async GetDataSet() {
        return [["1.12 ", 0], ["2.12", 25], ["3.12", 50], ["4.12", 75], ["5.12", 75], ["6.12", 75], ["7.12", 50], ["8.12", 25]];
    }

    static async SetContainedCo2(containedCo2) {
        if (Number.isNaN(containedCo2)) {
            return;
        }
        const result = fetch(this.apiAddress + "/api/v1/contained-co2", {
            method : 'POST',
            mode : 'cors',
            body : JSON.stringify({"contained_co2" : containedCo2})
        }).then(response => response.json());
        return result;
    }
}