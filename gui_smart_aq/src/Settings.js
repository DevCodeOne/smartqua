import React, { Component } from 'react'
import { Typography, TextField, Grid, Button } from "@material-ui/core";
import { ApiService } from './ApiService.js'

export class Settings extends Component {
    constructor(props) {
        super(props);
        this.state = { containedCo2 : "0", hostname : "first-co2-bottle-scale" }
    }

    componentDidMount() {
        this.timer = setInterval(() => {
            const data = ApiService.GetLoad();
            data.then(data => this.setState( { containedCo2 : data.contained_co2 , ... this.state}));
        }, 1000);
    }

    componentWillUnmount() {
        clearInterval(this.timer);
    }

    setContainedCo2 = () => {
        console.log("Setting contained co2");
        ApiService.SetContainedCo2(this.state.containedCo2);
    };

    setContainedCo2Value = (event) => {
        this.state.containedCo2 = event.target.value;
    };

    render() {
        return (
            <div>
                <Grid container direction="row" justify="center" alignItems="center" spacing={4}>
                    <Grid item xs={12} align="center">
                        <Typography variant="h2" align="center">
                            Settings
                        </Typography>
                    </Grid>

                    { /* Next row */ }
                    <Grid item xs={2} align="center" />
                    <Grid item xs={8} align="center">
                        <TextField id="outlined-basic" label={"Hostname : " + this.state.hostname} variant="outlined" fullWidth />
                    </Grid>
                    <Grid item xs={2} align="center" />

                    { /* Next row */ }
                    <Grid item xs={2} align="center" />
                    <Grid item xs={8} align="center">
                        <TextField id="outlined-basic" 
                        label={"Contained Co2 : " + this.state.containedCo2 + "g"} 
                        variant="outlined" 
                        fullWidth 
                        onChange={ (event) => { this.setContainedCo2Value(event) } }/>
                    </Grid>
                    <Grid item xs={2} align="center" />

                    { /* Next row */ }
                    <Grid item xs={3} align="center" />
                    <Grid item xs={3} align="center">
                        <Button variant="contained" color="primary">Tare</Button>
                    </Grid>
                    <Grid item xs={3} align="center">
                        <Button variant="contained" color="primary" onClick={ () => { this.setContainedCo2() } }>Save</Button>
                    </Grid>
                    <Grid item xs={3} align="center" />

                </Grid>
            </div>
        )
    }
}