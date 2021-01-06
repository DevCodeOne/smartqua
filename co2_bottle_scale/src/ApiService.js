export class ApiService {
    
    static apiAddress = "http://first-co2-bottle-scale.fritz.box"

    static async GetLoad() {
        const result = fetch(this.apiAddress + "/api/v1/load", { mode : 'cors'}).then(body => body.json());
        return result;
    }
}