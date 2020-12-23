import { Component } from 'preact';
import { ScaleContext } from './scalecontext'

export class Settings extends Component {

    static contextType = ScaleContext;

    static state = {
        apiAddress : "",
        containedCo2 : 0
    };

    onClick = (event) => {
        const { apiAddress } = this.context;

        fetch(apiAddress + "/api/v1/tare", {
            mode: 'cors',
            method: 'POST'
        })
            .then(response => response.json())
            .then(json => console.log(JSON.stringify(json)));
    }

    handleAddressChange = (event) => {
        this.setState({ apiAddress : event.target.value });
    }
    
    handleContainedCo2WeightChange = (event) => {
        const { apiAddress } = this.context;
        if (isNaN(event.target.value)) {
            return;
        }

        fetch(apiAddress + "/api/v1/contained-co2", {method: 'POST', mode : 'cors', body : JSON.stringify({ "contained_co2" : event.target.value })})
            .then(response => response.json())
            .then(() => { this.setState({ containedCo2 : event.target.value }); })
    }

    render() {
        const { apiAddress, containedCo2, setContainedCo2, setApiAddress } = this.context;

        return (
            <div class="container">
                <form>
                    <div class="row">
                        <h3> ApiAddress : { apiAddress } </h3>
                    </div>
                    <div class="row">
                        <h3> Contained Co2 : { containedCo2 } </h3>
                    </div>
                    <div class="row">
                        <div class="input-field col s12">
                            <input id="address" type="text" id="standard-basic" label="ApiAddress" onChange={this.handleAddressChange} value={this.state.apiAddress} />
                            <label for="address">ApiAddress</label>
                        </div>
                    </div>
                    <div class="row">
                        <div class="input-field" col s12>
                            <input id="weight" type="text" id="standard-basic" label="Co2 Weight" onChange={this.handleContainedCo2WeightChange} value={this.state.containedCo2}/>
                            <label for="weight">Contained Co2</label>
                        </div>
                    </div>
                    <div class="row">
                        <div class="col s6">
                            <a class="waves-effect waves-light btn" onclick={
                                (e) => {
                                    e.preventDefault();
                                    if (this.state.apiAddress != undefined && this.state.apiAddress != "") {
                                        setApiAddress(this.state.apiAddress);
                                    }

                                    if (this.state.containedCo2 != undefined && this.state.containedCo2 != "") {
                                        setContainedCo2(this.state.containedCo2);
                                    }
                                }
                            }>
                                Save Settings
                            </a>
                        </div>
                        <div class="col s6">
                            <a class="waves-effect waves-light btn" onclick={this.onClick}> Tare </a>
                        </div>
                    </div>
                </form>
            </div>
        )
    }
}
